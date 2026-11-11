#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import pathlib
import re
import shutil
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "../common"))
import installer

DEB_ARCH_TRIPLETS = {
    "amd64": "x86_64-linux-gnu",
    "arm64": "aarch64-linux-gnu",
    "armhf": "arm-linux-gnueabihf",
    "riscv64": "riscv64-linux-gnu",
}

GPU_WRAPPER_SCRIPT = """#!/bin/sh
if [ -x "$SNAP/gpu-2604/bin/gpu-2604-provider-wrapper" ]; then
  exec "$SNAP/gpu-2604/bin/gpu-2604-provider-wrapper" "$@"
fi
exec "$@"
"""

DESKTOP_LAUNCH_SCRIPT = """#!/bin/sh
if [ -x "$SNAP/gnome-platform/command-chain/desktop-launch" ]; then
  exec "$SNAP/gnome-platform/command-chain/desktop-launch" "$@"
fi
echo "gnome-core26 content snap is not connected" >&2
exit 1
"""

HOOKS_CONFIGURE_FONTS_SCRIPT = """#!/bin/sh
if [ -x "$SNAP/gnome-platform/command-chain/hooks-configure-fonts" ]; then
  exec "$SNAP/gnome-platform/command-chain/hooks-configure-fonts" "$@"
fi
exec "$@"
"""

# A no-op configure hook. snapd only runs hooks that exist in meta/hooks/, and
# the hook's work is done by its command-chain (hooks-configure-fonts, which
# rebuilds the font cache), so the hook itself does nothing. snapcraft
# generates the same stub (#!/bin/true) for hooks that only have a
# command-chain.
CONFIGURE_HOOK_SCRIPT = """#!/bin/true
"""


def parse_args() -> argparse.Namespace:
    parser = installer.parse_common_args()
    parser.add_argument(
        "--runtime-dir",
        help="Path to the unpacked snap runtime CIPD package.",
    )
    return parser.parse_args()


def get_icon_suffix(config: installer.InstallerConfig) -> str:
    if config.branding == "google_chrome":
        if config.channel == "beta":
            return "_beta"
        if config.channel == "unstable":
            return "_dev"
        if config.channel == "canary":
            return "_canary"
    return ""


def get_config_dir_name(config: installer.InstallerConfig) -> str:
    # The basename of the default user data directory.
    # LINT.IfChange(ConfigDirName)
    if config.branding == "chromium":
        return "chromium"
    assert config.branding == "google_chrome"
    suffix = {
        "stable": "",
        "beta": "-beta",
        "unstable": "-unstable",
        "canary": "-canary",
    }[config.channel]
    return f"google-chrome{suffix}"
    # LINT.ThenChange(
    #     //chrome/common/chrome_paths_linux.cc:DataDirBasename,
    #     //chrome/common/channel_info_posix.cc:ChannelSuffixForDataDir
    # )


def get_config_plug_name(config: installer.InstallerConfig) -> str:
    # The name of the personal-files plug used for importing an existing
    # profile. These match the plug names in Canonical's Chromium and Google
    # Chrome snaps, since store auto-connection is granted per plug name.
    if config.branding == "chromium":
        return "chromium-config"
    assert config.branding == "google_chrome"
    return "chrome-config"


def unwrap_cipd_path(path: pathlib.Path) -> pathlib.Path:
    """Follows .cipd/pkgs/... symlinks without dereferencing real ones."""
    while path.is_symlink():
        target = os.readlink(path)
        if "/.cipd/pkgs/" not in target and not target.startswith(
            ".cipd/pkgs/"
        ):
            break
        path = path.parent / target
    return path


def copy_runtime(runtime_dir: pathlib.Path, staging_dir: pathlib.Path) -> None:
    prime_dir = runtime_dir / "prime"
    if not prime_dir.is_dir():
        raise FileNotFoundError(
            f"Snap runtime directory not found: {prime_dir}. "
            'Set `"checkout_snap_runtime": True` in .gclient custom_vars '
            "and run `gclient sync`."
        )

    for root, dirs, files in os.walk(prime_dir):
        rel_root = pathlib.Path(root).relative_to(prime_dir)
        dst_root = staging_dir / rel_root
        dst_root.mkdir(parents=True, exist_ok=True)
        dst_root.chmod(installer.StandardPermissions.EXECUTABLE)

        kept_dirs = []
        for name in dirs:
            src_entry = unwrap_cipd_path(pathlib.Path(root, name))
            dst_entry = dst_root / name
            if src_entry.is_symlink():
                if dst_entry.is_symlink() or dst_entry.exists():
                    dst_entry.unlink()
                dst_entry.symlink_to(os.readlink(src_entry))
            else:
                dst_entry.mkdir(parents=True, exist_ok=True)
                dst_entry.chmod(installer.StandardPermissions.EXECUTABLE)
                kept_dirs.append(name)
        dirs[:] = kept_dirs

        for name in files:
            src_entry = unwrap_cipd_path(pathlib.Path(root, name))
            dst_entry = dst_root / name
            if dst_entry.is_symlink() or dst_entry.exists():
                dst_entry.unlink()
            if src_entry.is_symlink():
                target = os.readlink(src_entry)
                # Skip dangling absolute symlinks into paths core26 lacks
                # (matching craft-parts' cleanup).
                if target == "/etc/X11/rgb.txt":
                    continue
                dst_entry.symlink_to(target)
            else:
                shutil.copy2(src_entry, dst_entry)

    # The runtime package always contains copyright/ (see runtime/3pp/build.py).
    copyright_dir = runtime_dir / "copyright"
    assert copyright_dir.is_dir(), f"Missing {copyright_dir}"
    for entry in sorted(copyright_dir.iterdir()):
        src_file = unwrap_cipd_path(entry)
        if not src_file.is_file():
            continue
        dst_file = staging_dir / "usr/share/doc" / entry.name / "copyright"
        dst_file.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src_file, dst_file)
        dst_file.chmod(installer.StandardPermissions.REGULAR)


def render_manpage_to_text(manpage_text: str) -> str:
    """Converts the simple roff subset used in manpage.1.in to plain text."""
    out: list[str] = []
    indent = "    "
    next_is_tp_tag = False

    def clean_inline(text: str) -> str:
        text = re.sub(r"\\f[BIRP]", "", text)
        text = text.replace(r"\-", "-")
        text = text.replace(r"\&", "")
        return text

    for raw_line in manpage_text.splitlines():
        if raw_line.startswith(r'.\"'):
            continue
        if raw_line.startswith("."):
            parts = raw_line[1:].strip().split(None, 1)
            macro = parts[0] if parts else ""
            rest = clean_inline(parts[1]) if len(parts) > 1 else ""
            if macro == "TH":
                next_is_tp_tag = False
                continue
            if macro == "SH":
                if out and out[-1] != "":
                    out.append("")
                out.append(rest.strip('"'))
                next_is_tp_tag = False
                continue
            if macro in ("PP", "IP"):
                if out and out[-1] != "":
                    out.append("")
                next_is_tp_tag = False
                continue
            if macro == "TP":
                if out and out[-1] != "":
                    out.append("")
                next_is_tp_tag = True
                continue
            if macro in ("B", "I", "BR", "IR", "RB", "RI"):
                if macro in ("BR", "IR", "RB", "RI"):
                    rest = "".join(rest.split())
                prefix = "  " if next_is_tp_tag else indent
                out.append(f"{prefix}{rest}")
                next_is_tp_tag = False
                continue
            continue

        line = clean_inline(raw_line)
        if not line.strip():
            if out and out[-1] != "":
                out.append("")
            continue
        if next_is_tp_tag:
            out.append(f"  {line.strip()}")
            next_is_tp_tag = False
        else:
            out.append(f"{indent}{line}")

    return "\n".join(out).strip() + "\n"


def strip_comment_lines(text: str) -> str:
    lines = [
        line for line in text.splitlines() if not line.lstrip().startswith("#")
    ]
    return "\n".join(lines).rstrip() + "\n"


def write_executable(path: pathlib.Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    path.chmod(installer.StandardPermissions.EXECUTABLE)


def stage_snap_files(
    config: installer.InstallerConfig,
    script_dir: pathlib.Path,
    output_dir: pathlib.Path,
    staging_dir: pathlib.Path,
    runtime_dir: pathlib.Path,
) -> None:
    snap_name = config.info_vars["SNAPNAME"]
    install_dir_rel = config.info_vars["INSTALLDIR"].lstrip("/")
    install_dir = staging_dir / install_dir_rel
    triplet = DEB_ARCH_TRIPLETS[config.arch]

    # Only Qt 6 is bundled in the snap runtime.
    (install_dir / "libqt5_shim.so").unlink(missing_ok=True)

    # Snaps use the browser-support AppArmor/seccomp sandbox and must not ship
    # setuid binaries.
    sandbox_bin = install_dir / "chrome-sandbox"
    if sandbox_bin.exists():
        sandbox_bin.chmod(installer.StandardPermissions.EXECUTABLE)

    copy_runtime(runtime_dir, staging_dir)

    # Empty mount-point directories for content snaps and bind layouts.
    for rel_dir in (
        "data-dir/icons",
        "data-dir/sounds",
        "data-dir/themes",
        f"gnome-platform/usr/lib/{triplet}/libproxy",
        f"gnome-platform/usr/lib/{triplet}/webkit2gtk-4.1",
        f"gnome-platform/usr/lib/{triplet}/webkitgtk-6.0",
        "gnome-platform/usr/libexec/glycin-loaders",
        "gnome-platform/usr/share/xml/iso-codes",
        "gpu-2604",
        "usr/share/alsa",
    ):
        (staging_dir / rel_dir).mkdir(parents=True, exist_ok=True)

    # The app is named after the snap, so the snap's desktop file must be too.
    # Chrome derives its WM class from the desktop file name (see
    # chrome::GetDesktopName()).
    config.desktop_exec = snap_name
    config.desktop_icon = (
        f"${{SNAP}}/{install_dir_rel}/"
        f"product_logo_256{get_icon_suffix(config)}.png"
    )
    config.startup_wm_class = snap_name

    context = config.get_template_context()
    context.update(
        {
            "deb_arch_triplet": triplet,
            "snap_config_dir": get_config_dir_name(config),
            "snap_config_plug": get_config_plug_name(config),
        }
    )

    bin_dir = staging_dir / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)
    launcher = bin_dir / "chrome.launcher"
    installer.process_template(
        script_dir / "chrome.launcher.in", launcher, context
    )
    launcher.chmod(installer.StandardPermissions.EXECUTABLE)

    # Render help text for `-h` / `--help` (HandleHelpSwitches execs `man`)
    # without bundling GPL man-db or groff binaries.
    pkg_doc_dir = staging_dir / "usr/share/doc" / config.info_vars["PACKAGE"]
    pkg_doc_dir.mkdir(parents=True, exist_ok=True)
    raw_manpage = pkg_doc_dir / "manpage.1"
    installer.process_template(
        output_dir / "installer/common/manpage.1.in", raw_manpage, context
    )
    help_file = pkg_doc_dir / "help.txt"
    help_file.write_text(
        render_manpage_to_text(raw_manpage.read_text(encoding="utf-8")),
        encoding="utf-8",
    )
    help_file.chmod(installer.StandardPermissions.REGULAR)
    raw_manpage.unlink()
    pkg_name = config.info_vars["PACKAGE"]
    write_executable(
        bin_dir / "man",
        f'#!/bin/sh\nexec cat "$SNAP/usr/share/doc/{pkg_name}/help.txt"\n',
    )

    # Symlink xdg-email -> /usr/bin/xdg-open (provided by the core26 base snap)
    # so mailto: URLs work without bundling xdg-utils.
    usr_bin_dir = staging_dir / "usr/bin"
    usr_bin_dir.mkdir(parents=True, exist_ok=True)
    xdg_email = usr_bin_dir / "xdg-email"
    if xdg_email.is_symlink() or xdg_email.exists():
        xdg_email.unlink()
    xdg_email.symlink_to("/usr/bin/xdg-open")

    # Snap metadata, GUI desktop entry, configure hook, and command-chain.
    meta_dir = staging_dir / "meta"
    gui_dir = meta_dir / "gui"
    gui_dir.mkdir(parents=True, exist_ok=True)

    desktop_file = gui_dir / f"{snap_name}.desktop"
    installer.process_template(
        output_dir / "installer/common/desktop.template",
        desktop_file,
        context,
    )
    desktop_file.write_text(
        strip_comment_lines(desktop_file.read_text(encoding="utf-8")),
        encoding="utf-8",
    )
    desktop_file.chmod(installer.StandardPermissions.REGULAR)
    # Keep a copy in bin/ matching the legacy snapcraft layout.
    shutil.copyfile(desktop_file, bin_dir / desktop_file.name)
    (bin_dir / desktop_file.name).chmod(installer.StandardPermissions.REGULAR)

    snap_yaml = meta_dir / "snap.yaml"
    installer.process_template(script_dir / "snap.yaml.in", snap_yaml, context)
    snap_yaml.write_text(
        strip_comment_lines(snap_yaml.read_text(encoding="utf-8")),
        encoding="utf-8",
    )
    snap_yaml.chmod(installer.StandardPermissions.REGULAR)

    write_executable(meta_dir / "hooks/configure", CONFIGURE_HOOK_SCRIPT)
    command_chain_dir = staging_dir / "snap/command-chain"
    write_executable(command_chain_dir / "gpu-2604-wrapper", GPU_WRAPPER_SCRIPT)
    write_executable(
        command_chain_dir / "desktop-launch", DESKTOP_LAUNCH_SCRIPT
    )
    write_executable(
        command_chain_dir / "hooks-configure-fonts",
        HOOKS_CONFIGURE_FONTS_SCRIPT,
    )


def normalize_timestamps(root: pathlib.Path, timestamp: int) -> None:
    for dirpath, dirnames, filenames in os.walk(root, topdown=False):
        for name in filenames + dirnames:
            os.utime(
                os.path.join(dirpath, name),
                (timestamp, timestamp),
                follow_symlinks=False,
            )
    os.utime(root, (timestamp, timestamp), follow_symlinks=False)


def pack_snap(
    staging_dir: pathlib.Path, output_snap: pathlib.Path, build_timestamp: int
) -> None:
    normalize_timestamps(staging_dir, build_timestamp)
    tmp_snap = output_snap.with_name(output_snap.name + ".tmp")
    tmp_snap.unlink(missing_ok=True)
    # Matches the mksquashfs flags used by snapd's `snap pack` for app snaps.
    installer.run_command(
        [
            "mksquashfs",
            str(staging_dir),
            str(tmp_snap),
            "-noappend",
            "-comp",
            "lzo",
            "-no-fragments",
            "-no-progress",
            "-all-root",
            "-no-xattrs",
            "-mkfs-time",
            str(build_timestamp),
            "-root-time",
            str(build_timestamp),
        ]
    )
    tmp_snap.chmod(installer.StandardPermissions.REGULAR)
    tmp_snap.replace(output_snap)


def main() -> None:
    os.umask(0o022)
    args = parse_args()

    log_level = logging.INFO if os.environ.get("VERBOSE") else logging.ERROR
    logging.basicConfig(level=log_level, format="%(message)s")

    script_dir = pathlib.Path(__file__).parent.absolute()
    output_dir = pathlib.Path(args.output_dir).absolute()
    channel = args.channel
    runtime_dir = (
        pathlib.Path(args.runtime_dir).absolute()
        if args.runtime_dir
        else script_dir / "runtime" / f"linux-{args.arch}"
    )

    staging_dir = output_dir / f"snap-staging-{channel}"
    tmp_file_dir = output_dir / f"snap-tmp-{channel}"

    with installer.StagingContext(staging_dir, tmp_file_dir):
        config = installer.InstallerConfig.from_args(
            args, output_dir, package_format=installer.PackageFormat.SNAP
        )
        config.script_dir = script_dir
        config.staging_dir = staging_dir
        config.tmp_file_dir = tmp_file_dir
        config.shlib_perms = installer.StandardPermissions.REGULAR

        inst = installer.Installer(config)
        inst.prep_staging_common()
        inst.stage_install_common()

        stage_snap_files(
            config, script_dir, output_dir, staging_dir, runtime_dir
        )

        snap_name = config.info_vars["SNAPNAME"]
        package_file = (
            f"{snap_name}-{channel}_{config.version}_{args.arch}.snap"
        )
        pack_snap(staging_dir, output_dir / package_file, int(args.build_time))


if __name__ == "__main__":
    main()
