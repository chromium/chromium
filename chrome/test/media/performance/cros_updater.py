# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates ChromeOS sender DUTs to the latest Canary image over SSH."""

import logging
import os
import re
import subprocess
import sys
import tempfile
import time

import perf_config
import remote_transport
import senders

CROS_IMAGE_ARCHIVE_URL = 'gs://chromeos-image-archive'
_REBOOT_TIMEOUT_SECS = 300
_REBOOT_POLL_INTERVAL_SECS = 5
_GSUTIL_TIMEOUT_SECS = 60
_FLASH_TIMEOUT_SECS = 600


def _find_gsutil_cmd():
    """Returns the command prefix to invoke repo-local gsutil."""
    candidate = os.path.join(
        perf_config.REPO_ROOT,
        'third_party',
        'catapult',
        'third_party',
        'gsutil',
        'gsutil',
    )
    if os.path.exists(candidate):
        return [sys.executable, candidate]
    return ['gsutil']


def _run_gsutil(*gs_args):
    """Runs gsutil with `gs_args` and returns stripped stdout."""
    cmd = [*_find_gsutil_cmd(), *gs_args]
    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        timeout=_GSUTIL_TIMEOUT_SECS,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f'gsutil {" ".join(gs_args)} failed (rc={result.returncode}): '
            f'{(result.stderr or result.stdout or "").strip()}'
        )
    return result.stdout.strip()


def parse_lsb_release(text):
    """Returns (board, version) from ChromeOS /etc/lsb-release contents."""
    fields = {}
    for line in text.splitlines():
        if '=' in line:
            key, value = line.split('=', 1)
            fields[key.strip()] = value.strip()
    board = fields.get('CHROMEOS_RELEASE_BOARD', '')
    version = fields.get('CHROMEOS_RELEASE_VERSION', '')
    if not board or not version:
        raise RuntimeError(
            'Missing CHROMEOS_RELEASE_BOARD or CHROMEOS_RELEASE_VERSION in '
            f'/etc/lsb-release:\n{text}'
        )
    # Signed release boards have suffixes like 'dedede-signed-mp-v3keys'.
    board = board.split('-', 1)[0]
    return board, version


def parse_latest_build(text):
    """Returns (build_id, version) from a ChromeOS LATEST-main file."""
    build_id = text.strip()
    match = re.fullmatch(r'R\d+-(\d+\.\d+\.\d+)', build_id)
    if not match:
        raise RuntimeError(f'Unexpected LATEST-main format: {build_id!r}')
    return build_id, match.group(1)


def get_remote_cros_info(sender):
    """Queries (board, version) from /etc/lsb-release on the ChromeOS DUT."""
    result = sender.run('cat /etc/lsb-release')
    sender.transport.raise_if_disconnected(result, 'reading /etc/lsb-release')
    if result.returncode != 0:
        raise RuntimeError(
            f"Failed to read /etc/lsb-release on '{sender.transport.host}' "
            f'(rc={result.returncode}): {(result.stderr or "").strip()}'
        )
    return parse_lsb_release(result.stdout)


def get_latest_canary_build(board):
    """Fetches (build_id, version) from ChromeOS LATEST-main in GCS."""
    latest_url = f'{CROS_IMAGE_ARCHIVE_URL}/{board}-release/LATEST-main'
    return parse_latest_build(_run_gsutil('cat', latest_url))


def _get_target_partitions(sender):
    """Returns (current_root, target_kern, target_root)."""
    result = sender.run('rootdev -s')
    sender.transport.raise_if_disconnected(result, 'reading rootdev -s')
    if result.returncode != 0:
        raise RuntimeError(
            f"Failed to query rootdev on '{sender.transport.host}' "
            f'(rc={result.returncode}): {(result.stderr or "").strip()}'
        )
    current_root = result.stdout.strip()
    match = re.fullmatch(r'^(.*?)(\d+)$', current_root)
    if not match:
        raise RuntimeError(f'Unexpected rootdev path: {current_root!r}')
    prefix, root_num = match.group(1), int(match.group(2))
    if root_num == 3:
        return current_root, f'{prefix}4', f'{prefix}5'
    if root_num == 5:
        return current_root, f'{prefix}2', f'{prefix}3'
    raise RuntimeError(f'Unexpected active root partition: {current_root!r}')


def _stream_gs_to_ssh(sender, gs_url, remote_cmd):
    """Streams a GCS object directly into an SSH command's stdin."""
    gs_cmd = [*_find_gsutil_cmd(), '-q', 'cat', gs_url]
    # pylint: disable=protected-access
    ssh_cmd = sender.transport._ssh_argv(remote_cmd)
    with tempfile.TemporaryFile() as gs_stderr:
        with subprocess.Popen(
            gs_cmd, stdout=subprocess.PIPE, stderr=gs_stderr
        ) as gs_proc:
            try:
                with subprocess.Popen(
                    ssh_cmd,
                    stdin=gs_proc.stdout,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                ) as ssh_proc:
                    gs_proc.stdout.close()
                    try:
                        ssh_out, ssh_err = ssh_proc.communicate(
                            timeout=_FLASH_TIMEOUT_SECS
                        )
                        gs_rc = gs_proc.wait(timeout=_GSUTIL_TIMEOUT_SECS)
                    except subprocess.TimeoutExpired:
                        ssh_proc.kill()
                        gs_proc.kill()
                        ssh_proc.wait()
                        gs_proc.wait()
                        raise
            except BaseException:
                if gs_proc.poll() is None:
                    gs_proc.kill()
                    gs_proc.wait()
                raise
        gs_stderr.seek(0)
        gs_err = gs_stderr.read()

    if gs_rc != 0:
        raise RuntimeError(
            f'gsutil cat {gs_url} failed (rc={gs_rc}): '
            f'{gs_err.decode("utf-8", errors="replace").strip()}'
        )
    if ssh_proc.returncode != 0:
        detail = (ssh_err or ssh_out).decode('utf-8', errors='replace').strip()
        raise RuntimeError(
            f'Remote flash step failed (rc={ssh_proc.returncode}): {detail}'
        )


def _run_postinst(sender, target_root):
    """Runs postinst on `target_root` to activate the newly flashed slot."""
    result = sender.transport.run(
        f'TMP=$(mktemp -d) && mount -o ro {target_root} "$TMP" && '
        f'"$TMP/postinst" {target_root}; rc=$?; '
        'umount "$TMP" 2>/dev/null || true; '
        'rmdir "$TMP" 2>/dev/null || true; exit $rc',
        timeout=_FLASH_TIMEOUT_SECS,
    )
    sender.transport.raise_if_disconnected(
        result, 'running postinst on target rootfs'
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"Failed running postinst on target rootfs on "
            f"'{sender.transport.host}' (rc={result.returncode}): "
            f'{(result.stderr or result.stdout or "").strip()}'
        )


def _reboot_and_wait(sender):
    """Reboots the ChromeOS DUT and waits for SSH connectivity to return."""
    sender.run('nohup sh -c "sleep 1 && reboot" >/dev/null 2>&1 &')
    time.sleep(10)
    deadline = time.monotonic() + _REBOOT_TIMEOUT_SECS
    last_error = None
    while time.monotonic() < deadline:
        remote_transport.reset_connectivity_cache()
        try:
            sender.verify_connectivity()
            return get_remote_cros_info(sender)
        except Exception as e:
            last_error = e
            time.sleep(_REBOOT_POLL_INTERVAL_SECS)
    raise RuntimeError(
        f"Timed out waiting for '{sender.transport.host}' after reboot: "
        f'{last_error}'
    )


def _read_host_public_key():
    """Returns the host's ~/.ssh/id_ed25519.pub contents, or '' if absent."""
    pub_path = f'{remote_transport.SSH_KEY_PATH}.pub'
    if not os.path.exists(pub_path):
        return ''
    try:
        with open(pub_path, encoding='utf-8') as fp:
            return fp.read().strip()
    except OSError as e:
        logging.warning('Could not read %s: %s', pub_path, e)
        return ''


def _ensure_host_ssh_key_authorized(sender):
    """Restores the host SSH public key in /root/.ssh/authorized_keys."""
    pub_key = _read_host_public_key()
    if not pub_key:
        return
    # Extracting stateful.tgz wipes /root/.ssh/authorized_keys, leaving only
    # testing_rsa. Re-append the host's id_ed25519.pub so unpatched jobs and
    # direct SSH sessions continue to authenticate.
    escaped = pub_key.replace("'", "'\\''")
    cmd = (
        'mkdir -p /root/.ssh && chmod 700 /root/.ssh && '
        f"grep -qxF '{escaped}' /root/.ssh/authorized_keys 2>/dev/null || "
        f"printf '%s\\n' '{escaped}' >> /root/.ssh/authorized_keys && "
        'chmod 600 /root/.ssh/authorized_keys'
    )
    result = sender.run(cmd)
    if result.returncode != 0:
        logging.warning(
            "Failed to restore SSH public key on '%s' (rc=%d): %s",
            sender.transport.host,
            result.returncode,
            (result.stderr or '').strip(),
        )


def _revert_partial_update(sender, current_root):
    """Best-effort cleanup if the stateful step fails after postinst."""
    for action, cmd in (
        (
            'removing staged stateful update',
            'rm -rf /mnt/stateful_partition/.update_available '
            '/mnt/stateful_partition/var_new '
            '/mnt/stateful_partition/dev_image_new',
        ),
        ('reverting boot slot', f'/postinst {current_root}'),
    ):
        try:
            res = sender.run(cmd)
            if res.returncode != 0:
                logging.warning(
                    "Failed %s on '%s' (rc=%d): %s",
                    action,
                    sender.transport.host,
                    res.returncode,
                    (res.stderr or res.stdout or '').strip(),
                )
        except Exception as e:
            logging.warning(
                "Failed %s on '%s': %s",
                action,
                sender.transport.host,
                e,
            )


def ensure_latest_canary(args):
    """Flashes the ChromeOS DUT to LATEST-main if it is behind.

    Returns:
        True if the DUT was flashed, False if it was already up to date.
    """
    sender = senders.make_sender(args)
    sender.verify_connectivity()
    board, current_version = get_remote_cros_info(sender)
    latest_build_id, latest_version = get_latest_canary_build(board)
    if current_version == latest_version:
        if getattr(sender.transport, 'using_testing_rsa', False):
            _ensure_host_ssh_key_authorized(sender)
        logging.info(
            "ChromeOS DUT '%s' (%s) is already on latest Canary %s (%s).",
            args.sender,
            board,
            latest_version,
            latest_build_id,
        )
        return False

    gs_image_url = f'{CROS_IMAGE_ARCHIVE_URL}/{board}-release/{latest_build_id}'
    logging.info(
        "Updating ChromeOS DUT '%s' (%s) from %s to %s (%s)...",
        args.sender,
        board,
        current_version,
        latest_version,
        gs_image_url,
    )
    current_root, target_kern, target_root = _get_target_partitions(sender)
    _stream_gs_to_ssh(
        sender,
        f'{gs_image_url}/full_dev_part_KERN.bin.gz',
        f'gunzip -c | dd bs=1M of={target_kern}',
    )
    _stream_gs_to_ssh(
        sender,
        f'{gs_image_url}/full_dev_part_ROOT.bin.gz',
        f'gunzip -c | dd bs=1M of={target_root}',
    )
    _run_postinst(sender, target_root)
    try:
        _stream_gs_to_ssh(
            sender,
            f'{gs_image_url}/stateful.tgz',
            'tar --ignore-command-error --overwrite --selinux '
            '--directory /mnt/stateful_partition -xzf - && '
            'test -d /mnt/stateful_partition/var_new && '
            'test -d /mnt/stateful_partition/dev_image_new && '
            ': > /mnt/stateful_partition/.update_available',
        )
    except Exception:
        _revert_partial_update(sender, current_root)
        raise
    _, updated_version = _reboot_and_wait(sender)
    _ensure_host_ssh_key_authorized(sender)

    if updated_version != latest_version:
        raise RuntimeError(
            f"ChromeOS DUT '{args.sender}' reported version {updated_version} "
            f'after flashing {latest_build_id} (expected {latest_version}).'
        )
    logging.info(
        "Successfully updated ChromeOS DUT '%s' to %s.",
        args.sender,
        updated_version,
    )
    return True
