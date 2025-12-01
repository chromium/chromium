# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A common module for media performance tests."""

import logging
import multiprocessing
import os
import shutil
import subprocess
import sys
import time
import json
import urllib.request

from contextlib import AbstractContextManager

# pylint: disable=import-error, wrong-import-position
REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
MEASURES_ROOT = os.path.join(REPO_ROOT, 'build', 'util', 'lib', 'proto')
sys.path.append(MEASURES_ROOT)
import measures

CHROME_FUCHSIA_ROOT = os.path.join(REPO_ROOT, 'fuchsia_web', 'av_testing')
sys.path.append(CHROME_FUCHSIA_ROOT)
import server
import video_analyzer

TEST_SCRIPTS_ROOT = os.path.join(REPO_ROOT, 'build', 'fuchsia', 'test')
sys.path.append(TEST_SCRIPTS_ROOT)
from repeating_log import RepeatingLog
# pylint: enable=import-error, wrong-import-position

# --- Chrome for Testing Constants ---
CFT_JSON_URL = "https://googlechromelabs.github.io/chrome-for-testing/known-good-versions-with-downloads.json"

CHROMEDRIVER_PORT = int(os.environ.get('CHROMEDRIVER_PORT', '49573'))
SERVER_PORT = int(os.environ.get('SERVER_PORT', '8000'))

RECORDINGS_DIR = os.path.join(os.environ.get('ISOLATED_OUTDIR', '/tmp'),
                              'recordings')
LOCAL_HOST_IP = '127.0.0.1'
REMOTE_URL = f'http://{LOCAL_HOST_IP}:{CHROMEDRIVER_PORT}'

# This code is used as the default failure value for recordings in the case that
# `results.get()` throws an unexpected error. -128 is chosen as a clear fail
# case (large negative) that won't overly distort tracking graphs.
FAIL_CODE = -128

METRICS = [
    'smoothness',
    'freezing',
    'dropped_frame_count',
    'total_frame_count',
    'dropped_frame_percentage'
]

VIDEOS = [
    {
        'name': '1080p30fpsAV1_foodmarket_sync.mp4',
        'fps': 30
    },
    {
        'name': '1080p30fpsH264_foodmarket_yt_sync.mp4',
        'fps': 30
    },
    {
        'name': '1080p60fpsHEVC_boat_sync.mp4',
        'fps': 60
    },
    {
        'name': '1080p60fpsVP9_boat_yt_sync.webm',
        'fps': 60
    }
]


SENDER_CHROMEDRIVER_CHECK_CMD = {
    'mac': (
        'ps aux | grep chromedriver | grep -v grep'
    ),
    'win': (
        'powershell -Command "Get-Process -Name chromedriver -ErrorAction '
        'SilentlyContinue"'
    ),
}

SENDER_STATUS_CMD = {
    'mac': (
        f'curl '
        f'-s '
        f'-o /dev/null '
        f"-w '%{{http_code}}' "
        f'http://{LOCAL_HOST_IP}:{CHROMEDRIVER_PORT}/status'
    ),
    'win': (
        f'powershell -Command "(Invoke-WebRequest -Uri '
        f'http://{LOCAL_HOST_IP}:{CHROMEDRIVER_PORT}/status -UseBasicParsing '
        f'-ErrorAction SilentlyContinue).StatusCode"'
    ),
}

SENDER_TERMINATE_DRIVER_CMD = {
    'mac': (
        'killall chromedriver'
    ),
    'win': (
        'powershell -Command "Stop-Process -Name chromedriver -Force; '
        'taskkill /F /IM chromedriver.exe /t"'
    ),
}

WIN_REMOTE_TMP_DIR = 'C:\\cft_temp'


class StartProcess(AbstractContextManager):
  """Starts a multiprocessing.Process."""

  def __init__(self, target, args, terminate: bool):
    self._proc = multiprocessing.Process(target=target, args=args)
    self._terminate = terminate

  def __enter__(self):
    self._proc.start()

  def __exit__(self, exc_type, exc_value, traceback):
    if self._terminate:
      self._proc.terminate()
    self._proc.join()
    if not self._terminate:
      assert self._proc.exitcode == 0


# TODO: b/463482240
# Currently we rely on brittle string-matching to properly set v4l2 device
# dimensions. This should be removed as soon as we have a more robust option.
def _query_v4l2_device(device_path) -> tuple[int, int, int]:
  """Queries a video device using v4l2-ctl and returns its metrics."""
  logging.info('Querying video device status with v4l2-ctl for %s...',
               device_path)
  device_status_cmd = [
      'v4l2-ctl',
      f'--device={device_path}',
      '--all'
  ]
  try:
    result = subprocess.run(device_status_cmd, check=True,
                            capture_output=True, text=True)
    logging.info('v4l2-ctl output:\n%s', result.stdout)

    width, height, fps = 0, 0, 0
    for line in result.stdout.splitlines():
      if 'Width/Height' in line:
        parts = line.split(':')[-1].strip().split('/')
        width = int(parts[0])
        height = int(parts[1])
      elif 'Frames per second' in line:
        fps = int(float(line.split(':')[1].strip().split(' ')[0]))
    if not (width and height and fps):
      raise RuntimeError(f'Could not parse width, height, or fps from '
                         f'v4l2-ctl output:\n{result.stdout}')
    return width, height, fps
  except (subprocess.CalledProcessError, FileNotFoundError) as e:
    stderr = e.stderr if hasattr(e, 'stderr') else '(no stderr)'
    stdout = e.stdout if hasattr(e, 'stdout') else '(no stdout)'
    raise RuntimeError(f'Could not query device status for {device_path}. '
                       f'Stdout: {stdout}\nStderr: {stderr}') from e


def send_ssh_command(hostname, username, command, blocking=False):
  """
    Sends a command to a remote host via SSH.

    Args:
        hostname (str): The remote host to connect to.
        username (str): The username for the SSH connection.
        command (str): The command to execute on the remote host.
        blocking (bool): If True, waits for the command to complete.
                         If False, runs the command in a non-blocking way.

    Returns:
        subprocess.CompletedProcess or subprocess.Popen: The process object.
    """
  key_path = os.path.expanduser('~/.ssh/id_ed25519')
  ssh_command = ['ssh', '-i', key_path, f'{username}@{hostname}', command]
  logging.debug('Executing SSH command: %s', ' '.join(ssh_command))

  if blocking:
    process = subprocess.run(ssh_command,
                             capture_output=True,
                             text=True,
                             timeout=60,
                             check=False)
  else:
    process = subprocess.Popen(  # pylint: disable=consider-using-with
        ssh_command,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True)

  return process


def terminate_old_chromedriver(args):
  """Tries to terminate any existing chromedriver processes."""
  logging.info("Attempting to terminate old chromedriver processes...")
  send_ssh_command(args.sender, args.username,
                   SENDER_TERMINATE_DRIVER_CMD[args.sender_os])

  for _ in range(5):
    result = send_ssh_command(args.sender,
                              args.username,
                              SENDER_CHROMEDRIVER_CHECK_CMD[args.sender_os],
                              blocking=True)
    if not result.stdout.strip():
      logging.info("Old chromedriver processes confirmed gone.")
      return
    logging.info("Old chromedriver processes still present, waiting...")
    time.sleep(1)
  raise RuntimeError("Chromedriver processes lingered after kill attempts.")


def download_cft_urls(sender_os, version=None):
  """
    Downloads the CfT JSON and finds the URLs for a specific version.
    """
  logging.info("Downloading Chrome for Testing JSON data...")
  with urllib.request.urlopen(CFT_JSON_URL) as url:
    data = json.loads(url.read().decode())

  platform = {
      'mac': (
          'mac-arm64'
      ),
      'win': (
          'win64'
      ),
  }

  for v in reversed(data['versions']):
    if not version or v['version'] == version:
      chrome_url = None
      driver_url = None
      for download in v['downloads']['chrome']:
        if download['platform'] == platform[sender_os]:
          chrome_url = download['url']
      for download in v['downloads']['chromedriver']:
        if download['platform'] == platform[sender_os]:
          driver_url = download['url']
      if chrome_url and driver_url:
        logging.info("Found URLs for version %s", v['version'])
        return chrome_url, driver_url

  raise RuntimeError(f"Could not find downloads for version {version}")


def install_and_setup_chrome(args, chrome_version):
  """
    Downloads and sets up a specific version of Chrome for Testing and its
    matching chromedriver.
    """
  chrome_url, driver_url = download_cft_urls(args.sender_os, chrome_version)
  chrome_zip = chrome_url.split('/')[-1]
  driver_zip = driver_url.split('/')[-1]
  chrome_unzip_dir = chrome_zip.replace('.zip', '')
  driver_unzip_dir = driver_zip.replace('.zip', '')
  # --- Download and Unzip on Remote ---
  logging.info("Downloading Chrome and Chromedriver on remote machine.")
  if args.sender_os == 'mac':
    remote_tmp_dir = '/tmp'
    download_commands = (
        f"curl -L {chrome_url} -o {remote_tmp_dir}/{chrome_zip} && "
        f"curl -L {driver_url} -o {remote_tmp_dir}/{driver_zip} && "
        f"unzip -o {remote_tmp_dir}/{chrome_zip} -d {remote_tmp_dir} && "
        f"unzip -o {remote_tmp_dir}/{driver_zip} -d {remote_tmp_dir}")
    send_ssh_command(args.sender,
                     args.username,
                     download_commands,
                     blocking=True)
    remote_app_path = (
        f'{remote_tmp_dir}/{chrome_unzip_dir}/Google Chrome for '
        'Testing.app')
    remote_chromedriver_path = (
        f'{remote_tmp_dir}/{driver_unzip_dir}/chromedriver')

    chmod_command = f'chmod +x {remote_chromedriver_path}'
    send_ssh_command(args.sender, args.username, chmod_command, blocking=True)

    start_driver_cmd = (
        f'nohup {remote_chromedriver_path} --port={CHROMEDRIVER_PORT} '
        f'--allowed-origins=\"*\" '
        f'--verbose '
        f'--log-path=/tmp/chromedriver_verbose.log '
        f'--enable-chrome-logs '
        f'> /dev/null 2>&1 &'
    )
    send_ssh_command(args.sender, args.username, start_driver_cmd)

  elif args.sender_os == 'win':
    logging.info("Windows OS detected. Implementing install and setup.")
    remote_tmp_dir = WIN_REMOTE_TMP_DIR
    send_ssh_command(args.sender,
                     args.username,
                     f'if not exist {remote_tmp_dir} mkdir {remote_tmp_dir}',
                     blocking=True)
    logging.info("Using remote_tmp_dir: %s", remote_tmp_dir)
    chrome_zip_path = f'{remote_tmp_dir}\\{chrome_zip}'
    driver_zip_path = f'{remote_tmp_dir}\\{driver_zip}'

    download_commands = (
        f"powershell -Command \"Start-BitsTransfer -Source '{chrome_url}' "
        f"-Destination '{chrome_zip_path}'; "
        f"Start-BitsTransfer -Source '{driver_url}' "
        f"-Destination '{driver_zip_path}'\""
    )
    logging.info("Downloading remote files...")
    send_ssh_command(args.sender,
                     args.username,
                     download_commands,
                     blocking=True)

    unzip_commands = (
        f"powershell -Command \"Expand-Archive -Path '{chrome_zip_path}' "
        f"-DestinationPath '{remote_tmp_dir}' -Force; "
        f"Expand-Archive -Path '{driver_zip_path}' "
        f"-DestinationPath '{remote_tmp_dir}' -Force\""
    )
    logging.info("Unzipping remote files...")
    send_ssh_command(args.sender,
                     args.username,
                     unzip_commands,
                     blocking=True)

    # Determine actual paths after unzipping (no nesting)
    remote_app_path = f'{remote_tmp_dir}\\chrome-win64\\chrome.exe'
    remote_chromedriver_dir = f'{remote_tmp_dir}\\chromedriver-win64'
    remote_chromedriver_path = (
        f'{remote_chromedriver_dir}\\chromedriver.exe')

    # Create and run the batch script
    batch_script_path = f'{remote_tmp_dir}\\start_chromedriver.bat'
    batch_script_content = (
        f'set PATH=%PATH%;{remote_chromedriver_dir}\n'
        f'cd /d "{remote_chromedriver_dir}"\n'
        f'"{remote_chromedriver_path}" --port={CHROMEDRIVER_PORT} '
        f'--disable-ipv6 '
        f'--allowed-origins=* --allowed-ips= --verbose '
        f'--log-path="{remote_chromedriver_dir}\\chromedriver_verbose.log" '
        f'--enable-chrome-logs > '
        f'"{remote_chromedriver_dir}\\chromedriver_console.log" 2>&1\n'
    )

    create_script_cmd = (
        f"powershell -Command \"'{batch_script_content}' | "
        f"Out-File -FilePath '{batch_script_path}' -Encoding ascii\""
    )
    logging.info("Creating remote batch script...")
    send_ssh_command(args.sender,
                     args.username,
                     create_script_cmd,
                     blocking=True)

    logging.info("Scheduling and running a task to start chromedriver...")
    delete_task_cmd = 'schtasks /delete /tn StartChromeDriverTask /f'
    send_ssh_command(args.sender, args.username, delete_task_cmd, blocking=True)

    create_task_cmd = (
        f'schtasks /create /tn StartChromeDriverTask /tr "{batch_script_path}" '
        '/sc ONCE /st 23:59 /f'
    )
    send_ssh_command(args.sender, args.username, create_task_cmd, blocking=True)

    run_task_cmd = 'schtasks /run /tn StartChromeDriverTask'
    send_ssh_command(args.sender, args.username, run_task_cmd, blocking=True)

  logging.info("Finished chromedriver setup attempt.")
  return remote_app_path


def wait_for_chromedriver(args):
  """Waits for the new chromedriver to be ready by checking its status URL."""
  logging.info("Starting Chromedriver status check...")
  for i in range(10):
    try:
      result = send_ssh_command(args.sender,
                                args.username,
                                SENDER_STATUS_CMD[args.sender_os],
                                blocking=True)
      stdout = result.stdout.strip()
      if result.returncode == 0 and stdout == '200':
        logging.info("Chromedriver is ready.")
        return
      logging.warning(
          f"Attempt {i+1} failed. Chromedriver not "
          f"ready. "
          f"Return code: {result.returncode}, "
          f"stdout: '{stdout}', "
          f"stderr: '{result.stderr.strip()}'")
    except subprocess.TimeoutExpired:
      logging.warning("Status check timed out. Retrying...")
    except Exception as e:  # pylint: disable=broad-exception-caught
      logging.warning("A script-level error occurred: %s. Retrying...", e)
    time.sleep(2)

  raise RuntimeError("Chromedriver still not ready after multiple attempts.")

def start_ssh_tunnel(args):
  # pylint: disable=consider-using-with
  """Starts the SSH tunnel process."""
  host_tunnel_cmd = [
      'ssh',
      '-i',
      f'~/.ssh/id_ed25519',
      '-L',
    f'{CHROMEDRIVER_PORT}:{LOCAL_HOST_IP}:{CHROMEDRIVER_PORT}',
      '-R',
      f'{SERVER_PORT}:{LOCAL_HOST_IP}:{SERVER_PORT}',
      f'{args.username}@{args.sender}',
      '-N'
  ]
  tunnel_proc = subprocess.Popen(host_tunnel_cmd)
  logging.info("Started tunnel.")
  return tunnel_proc

def teardown_recording_process(rec_proc):
  """
    Tears down the recording process.

    This function safely tears down the ffmpeg recording process via either
    a graceful wait or a forceful terminate.

    Args:
        rec_proc (subprocess.Popen): The video recording process.
    """
  if rec_proc is not None:
    logging.info("Waiting for recording to finish...")
    try:
      rec_proc.communicate(timeout=20)
      logging.info("Recording finished.")
    except subprocess.TimeoutExpired as e:
      logging.warning("WARNING: Recording process timed out after 20 "
                      "seconds. Terminating it now.")
      rec_proc.terminate()
      rec_proc.wait()
      raise RuntimeError("Recording process timed out and was "
                         "forcefully terminated.") from e

def teardown_test_environment(driver, tunnel_proc, args):
  """
    Tears down the test environment, ensuring the driver and tunnel are safely
    terminated.

    This function safely terminates the the Selenium WebDriver, and the SSH
    tunnel. It handles timeouts gracefully and ensures resources are released
    properly.

    Args:
        driver (webdriver.Remote): The Selenium WebDriver instance.
        tunnel_proc (subprocess.Popen): The SSH tunnel process.
        args: The parsed command-line arguments.
    """
  if driver:
    driver.quit()
    logging.info("Terminated chromedriver.")

  if tunnel_proc and tunnel_proc.poll() is None:
    tunnel_proc.terminate()
    logging.info("Terminated tunnel.")

  cleanup_command = {
      'mac': (
          f"rm -rf /tmp/chrome-mac-arm64 /tmp/chromedriver-mac-arm64 "
          f"/tmp/*.zip"
      ),
      'win': (
          f'powershell -Command "Remove-Item -Path {WIN_REMOTE_TMP_DIR} '
          '-Recurse -Force -ErrorAction SilentlyContinue"'
      ),
  }

  send_ssh_command(args.sender, args.username,
                   cleanup_command[args.sender_os])
  logging.info("Cleaned up tmp files on remote machine.")
