# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import selenium
import os
import shutil
import subprocess
import socket
import time
import multiprocessing
import logging
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.chrome.options import Options as ChromeOptions
from contextlib import AbstractContextManager

MEASURES_ROOT = os.path.join(os.path.dirname(__file__), '..', '..', '..',
                             '..', 'build', 'util', 'lib', 'proto')
sys.path.append(MEASURES_ROOT)
import measures

CHROME_FUCHSIA_ROOT = os.path.join(os.path.dirname(__file__), '..', '..',
                                   '..', '..', 'fuchsia_web', 'av_testing')
sys.path.append(CHROME_FUCHSIA_ROOT)
import server
import video_analyzer

TEST_SCRIPTS_ROOT = os.path.join(os.path.dirname(__file__), '..', '..',
                                 'build', 'fuchsia', 'test')
sys.path.append(TEST_SCRIPTS_ROOT)
from repeating_log import RepeatingLog

_CHROMEDRIVER_PORT = int(os.environ.get('CHROMEDRIVER_PORT', '49573'))
_SENDER = os.environ.get('SENDER')
_USERNAME = os.environ.get('USERNAME')
_PASSWORD = os.environ.get('PASSWORD')
_RECEIVER = os.environ.get('RECEIVER')
_SERVER_PORT = int(os.environ.get('SERVER_PORT', '8000'))

recordings_dir = os.path.join(os.environ.get('TMPDIR', '/tmp'), 'recordings')
REMOTE_URL = f'http://127.0.0.1:{_CHROMEDRIVER_PORT}'
tunnel_proc = None
rec_proc = None

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

host_tunnel_cmd = [
    'sshpass',
    '-p',
    _PASSWORD,
    'ssh',
    '-L',
    f'{_CHROMEDRIVER_PORT}:127.0.0.1:{_CHROMEDRIVER_PORT}',
    f'{_USERNAME}@{_SENDER}',
    '-N'
]

sender_chromedriver_cmd = (
    f'nohup /opt/homebrew/bin/chromedriver --port={_CHROMEDRIVER_PORT} '
    f'--allowed-origins=\"*\" '
    f'--verbose '
    f'--log-path=/tmp/chromedriver_verbose.log '
    f'--enable-chrome-logs '
    f'> /dev/null 2>&1 &'
)

sender_chromedriver_check_cmd = (
    f'ps aux | grep chromedriver | grep -v grep'
)

sender_terminate_driver_cmd = (
    f'killall chromedriver'
)

sender_status_cmd = (
    f'curl '
    f'-s '
    f'-o /dev/null '
    f'-w "%{{http_code}}" '
    f'http://127.0.0.1:{_CHROMEDRIVER_PORT}/status'
)

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

def send_ssh_command(hostname, _USERNAME, _PASSWORD, command, blocking=False):
    ssh_command = ['sshpass', '-p', _PASSWORD, 'ssh',
                   f'{_USERNAME}@{hostname}', command]
    if blocking:
        process = subprocess.run(
            ssh_command,
            capture_output=True,
            text=True,
            timeout=60
        )
    else:
        process = subprocess.Popen(
            ssh_command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

    return process

def setup_test_environment():
    send_ssh_command(_SENDER, _USERNAME, _PASSWORD,
                     sender_terminate_driver_cmd)
    time.sleep(1)

    for _ in range(5):
        result = send_ssh_command(_SENDER, _USERNAME, _PASSWORD,
                                  sender_chromedriver_check_cmd, blocking=True)

        if not result.stdout.strip():
            logging.info("DEBUG: Old chromedriver processes confirmed gone.")
            break
        logging.info("DEBUG: Old chromedriver processes still present, "
                     "waiting...")
        time.sleep(1)
    else:
        raise RuntimeError("Old chromedriver processes lingered even after "
                           "kill attempts.")

    send_ssh_command(_SENDER, _USERNAME, _PASSWORD, sender_chromedriver_cmd)
    logging.info("Started chromedriver.")

    logging.info("Starting Chromedriver status check...")
    for attempt in range(5):
        try:
            result = send_ssh_command(_SENDER, _USERNAME, _PASSWORD,
                                      sender_status_cmd, blocking=True)
            stdout = result.stdout.strip()

            if result.returncode == 0 and stdout == '200':
                logging.info(f"SUCCESS: Attempt {attempt + 1}: "
                             "Chromedriver is ready.")
                break
            elif result.returncode == 7:
                logging.info(f"info: Attempt {attempt + 1}: Connection refused "
                             "(curl code 7). Chromedriver not ready yet...")
            else:
                logging.warn(f"ERROR: Attempt {attempt + 1}: An unexpected "
                             "error occurred!")
                logging.warn(f"    --> Exit Code: {result.returncode}")
                logging.warn(f"    --> Stderr: {result.stderr.strip()}")
                logging.warn(f"    --> Stdout: {stdout}")
        except subprocess.TimeoutExpired:
            logging.warn(f"ERROR: Attempt {attempt + 1}: Status check timed "
                         "out. Retrying...")
        except Exception as e:
            logging.warn(f"ERROR: Attempt {attempt + 1}: A script-level error "
                         f"occurred: {e}. Retrying...")

        time.sleep(2)
    else:
        raise RuntimeError("Chromedriver did not become ready after multiple "
                           "attempts.")

    chrome_options = ChromeOptions()
    chrome_options.add_argument("--enable-features=CastMediaRouteProvider,"
                                "DialMediaRouteProvider")
    chrome_options.add_argument("--enable-logging=stderr")
    chrome_options.add_argument("--media-router-cast-allow-all-ips")
    chrome_options.add_argument("--v=1")
    chrome_options.add_argument("--vmodule=media_router*=2,"
                                "discovery_mdns*=2,cast_channel*=2,"
                                "webrtc_logging/*=2")
    chrome_options.add_argument('--password-store=basic')

    tunnel_proc = subprocess.Popen(host_tunnel_cmd)
    logging.info("Started tunnel.")

    logging.info(f"Attempting connection to {REMOTE_URL}.")

    driver = None
    for i in range(20):
        try:
            driver = webdriver.Remote(
                command_executor=REMOTE_URL,
                options=chrome_options
            )
            logging.info("Successfully connected!")
            break
        except:
            logging.info("Tunnel not yet up. Sleeping ...")
            time.sleep(2)
    return driver, tunnel_proc

def teardown_test_environment(driver, tunnel_proc, rec_proc):
    if rec_proc is not None:
        while rec_proc.poll() == None:
            logging.info("Still recording...")
            time.sleep(2)
        logging.info("Recording finished.")

    if driver:
        driver.quit()
        logging.info("Terminated chromedriver.")

    if tunnel_proc and tunnel_proc.poll() is None:
        tunnel_proc.terminate()
        logging.info("Terminated tunnel.")

def run_performance_test(video_file: str, framerate: int, driver: webdriver):
    # force video output to mp4
    output_file = os.path.join(recordings_dir,
                               video_file.replace('.webm', '.mp4'))
    host_recording_cmd = [
        'ffmpeg',
        '-y',
        '-f', 'video4linux2',
        '-framerate', str(framerate),
        '-video_size', '3840x2160',
        '-input_format', 'yuyv422',
        '-i', '/dev/video0',
        '-thread_queue_size', '1024',
        '-c:v', 'libx264',
        '-preset', 'ultrafast',
        '-crf', '28',
        '-pix_fmt', 'yuv420p',
        '-g', '60',
        '-t', '35',
        output_file
    ]

    driver.get("https://storage.googleapis.com/castapi/CastHelloVideo/"
               "index.html")
    time.sleep(2)
    button_xpath = "//button[text()='Launch app']"
    button = driver.find_element(By.XPATH, button_xpath)
    button.click()
    time.sleep(2)
    driver.get(f'http://{socket.gethostbyname(socket.gethostname())}:'
               f'{_SERVER_PORT}/video.html?file={video_file}')

    casting_initiated = False

    try:
        logging.info("Enabling Cast discovery via CDP...")
        enable_result = driver.execute_cdp_cmd("Cast.enable",
                                               {"presentationUrl": ""})

        time.sleep(5)

        # start recording
        rec_proc_local = subprocess.Popen(host_recording_cmd,
                                          stdout=subprocess.PIPE,
                                          stderr=subprocess.PIPE,
                                          text=True)

        logging.info("ffmpeg recording process started. Waiting for 'Stream "
                     "mapping:' confirmation...")

        while True:
            line = rec_proc_local.stderr.readline() # Use local variable
            if line:
                line = line.strip()
                logging.info(f"FFMPEG STARTUP: {line}")
                if "Stream mapping:" in line:
                    logging.info("Started recording.")
                    break

        logging.info(f"Attempting to start tab mirroring directly to "
                     f"'{_RECEIVER}'...")
        driver.execute_cdp_cmd("Cast.startTabMirroring",
                               {"sinkName": _RECEIVER})
        casting_initiated = True
        logging.info(f"'Cast.startTabMirroring' command sent to {_RECEIVER}.")

        def _wait_js_condition(driver, element, condition: str) -> bool:
            """Waits a condition on the element once a second for at most 30
            seconds, returns True if the condition met."""
            start = time.time()
            while not driver.execute_script(f'return arguments[0].{condition};',
                                            element):
                if time.time() - start >= 30:
                    return False
                time.sleep(1)
            return True

        video = driver.find_element(By.ID, 'video')

        with measures.time_consumption(video_file, 'video_perf', 'playback',
                                       'loading'), \
             RepeatingLog(f'Waiting for video {video_file} to be loaded.'):
            if not _wait_js_condition(driver, video, 'readyState >= 2'):
                logging.warning(
                    '%s may never be loaded, still go ahead to play it.',
                    video_file)
                measures.average(video_file, 'video_perf', 'playback',
                                 'failed_to_load').record(1)

        video.click()
        logging.info("Started playing video.")

        logging.info("Casting for 30 seconds (script will then stop casting "
                     "and quit)...")
        time.sleep(30)

        while rec_proc_local.poll() == None: # Use local variable
            logging.info("still recording...")
            time.sleep(2)
        logging.info("recording finished.")

        results = video_analyzer.from_original_video(
            output_file, f"/usr/local/cipd/videostack_videos_30s/{video_file}")

        if not results:
            raise RuntimeError("Missing video analyzer results. See log for "
                               "further details.")

        def record(key: str) -> None:
            # If the video_analyzer does not generate any result, treat it as an
            # error and use the default value to filter them out instead of
            # failing the tests.
            # TODO(crbug.com/40935291): Revise the default value for errors.
            measures.average(video_file, 'video_perf', key).record(
                results.get(key, -128))

        record('smoothness')
        record('freezing')
        record('dropped_frame_count')
        record('total_frame_count')
        record('dropped_frame_percentage')
        logging.warning('Video analysis result of %s: %s', video_file, results)

    except Exception as e:
            raise RuntimeError(f"Error during CDP Cast command: {e}\nCheck "
                               "the chromedriver log on the remote laptop for "
                               "more details.")
    finally:
        if driver:
            if casting_initiated and _RECEIVER:
                logging.info(f"Attempting to stop casting to '{_RECEIVER}'...")
                try:
                    driver.execute_cdp_cmd("Cast.stopCasting",
                                           {"sinkName": _RECEIVER})
                    logging.info("'Cast.stopCasting' command sent.")
                except Exception as stop_cast_error:
                    raise RuntimeError(f"Error stopping cast: "
                                       f"{stop_cast_error}")
    return rec_proc_local # Return the local rec_proc

def main():
    logging.basicConfig(level=logging.INFO)

    if os.path.exists(recordings_dir):
        shutil.rmtree(recordings_dir)
    os.makedirs(recordings_dir)
    for video in VIDEOS:
        driver = None
        tunnel_proc_instance = None
        rec_proc_instance = None

        try:
            driver, tunnel_proc_instance = setup_test_environment()
            rec_proc_instance = run_performance_test(video['name'],
                                                     video['fps'], driver)
        except Exception as e:
            raise RuntimeError(f"Error during video {video['name']} test: {e}")
        finally:
            teardown_test_environment(driver, tunnel_proc_instance,
                                      rec_proc_instance)

if __name__ == '__main__':
    with StartProcess(server.start, [ _SERVER_PORT ], True):
        sys.exit(main())