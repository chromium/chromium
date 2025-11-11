#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Orchestrates local policy testing on a ChromeOS test device.

This script automates the entire workflow for applying local policies using a
fake_dmserver. It performs the following steps:
1.  Generates the necessary policy blob from a user-friendly JSON file.
2.  Starts the fake_dmserver process to serve the policies.
3.  Configures Chrome to use the local server.
4.  Restarts the Chrome UI to ensure the new policies are fetched and applied.
5.  Monitors the input policy file for changes and automatically re-applies
    them.
"""

import argparse
import atexit
import base64
import json
import logging
import os
import re
import signal
import subprocess
import sys
import time
import urllib.error
import urllib.request

from blob_generator import POLICY_TEST_TOOL_PATH
# Add the new consolidated directory to sys.path.
sys.path.insert(0, POLICY_TEST_TOOL_PATH)

logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')

try:
  from blob_generator import generate_device_policy_schema
  from blob_generator import apply_user_policies
  from blob_generator import apply_device_policies
  import chrome_device_policy_pb2
  import chrome_settings_pb2
  import policy_common_definitions_pb2
except ImportError as e:
  logging.critical(f"Failed to import Chrome policy protobuf modules: {e}")
  sys.exit(1)

FAKE_DMSERVER_PATH = "/usr/local/libexec/chrome-binary-tests/fake_dmserver"
PERSISTENT_DATA_DIR = "/var/tmp/dmserver_data"
MANUAL_MAP_PATH = (
    f"{POLICY_TEST_TOOL_PATH}/manual_device_policy_proto_map.yaml")
CHROME_DEV_CONFIG_PATH = "/etc/chrome_dev.conf"


class Orchestrator:
  """Orchestrates the fake_dmserver and Chrome configuration."""

  def __init__(self, policy_file, chrome_flags=None):
    self.policy_file = policy_file
    self.chrome_flags = chrome_flags or []
    self.dmserver_process = None

  def generate_policy_blob(self, input_path, output_dir):
    """Calls the generator script to convert simple JSON to a policy blob."""
    output_path = os.path.join(output_dir, "policy.json")
    try:
      with open(input_path, "r", encoding="utf-8") as f:
        simple_policies = json.load(f)
    except json.JSONDecodeError as e:
      raise ValueError(f"Invalid JSON in {input_path}: {e}") from e

    device_schema = generate_device_policy_schema(MANUAL_MAP_PATH)
    if not device_schema:
      raise RuntimeError("Failed to generate device policy schema.")

    policy_blob = {}

    policy_user = simple_policies.get("policy_user")
    if not policy_user:
      raise ValueError("'policy_user' must be defined in the policy file.")
    policy_blob["policy_user"] = policy_user

    managed_users = simple_policies.get("managed_users", ["*"])
    policy_blob["managed_users"] = managed_users

    policy_blob["policies"] = []

    optional_params = [
        "allow_set_device_attributes",
        "current_key_index",
        "device_affiliation_ids",
        "directory_api_id",
        "initial_enrollment_state",
        "request_errors",
        "robot_api_auth_code",
        "user_affiliation_ids",
        "use_universal_signing_keys",
    ]
    for param in optional_params:
      if param in simple_policies:
        policy_blob[param] = simple_policies[param]

    if "user" in simple_policies:
      user_settings = chrome_settings_pb2.ChromeSettingsProto()
      apply_user_policies(simple_policies["user"], user_settings)
      encoded_policy = base64.b64encode(
          user_settings.SerializeToString()).decode("utf-8")
      policy_blob["policies"].append({
          "policy_type": "google/chromeos/user",
          "value": encoded_policy
      })

    if "device" in simple_policies:
      device_settings = chrome_device_policy_pb2.ChromeDeviceSettingsProto()
      apply_device_policies(simple_policies["device"], device_settings,
                            device_schema)
      encoded_policy = base64.b64encode(
          device_settings.SerializeToString()).decode("utf-8")
      policy_blob["policies"].append({
          "policy_type": "google/chromeos/device",
          "value": encoded_policy
      })

    with open(output_path, "w", encoding="utf-8") as f:
      json.dump(policy_blob, f, indent=2)

    logging.info(f"Successfully wrote policy blob to {output_path}")
    return True

  def configure_chrome_for_local_server(self, server_url):
    """Writes the device management URL to the Chrome dev config file."""
    timestamp = time.strftime("%Y%m%d-%H%M%S")
    timestamped_backup_path = (f"{CHROME_DEV_CONFIG_PATH}.original.{timestamp}")

    # Back up the original file to restore it later during cleanup.
    if os.path.exists(CHROME_DEV_CONFIG_PATH):
      os.rename(CHROME_DEV_CONFIG_PATH, timestamped_backup_path)
      logging.info(f"Backed up original {CHROME_DEV_CONFIG_PATH} to "
                   f"{timestamped_backup_path}")

    REMOTE_DEBUGGING_PORT = 9224
    chrome_flags = [
        f"--device-management-url={server_url}",
        f"--remote-debugging-port={REMOTE_DEBUGGING_PORT}",
        "--enable-devtools-pwa-handler",
        "--force-devtools-available",
        "--enable-features=IsolatedWebAppDevMode",
        "--ignore-urlfetcher-cert-requests",
        "--enterprise-enable-initial-enrollment=never",
        "--enterprise-enable-state-determination=never",
        "--enterprise-enrollment-skip-robot-auth",
        "--policy-fetch-timeout=1",
        "--disable-policy-key-verification",
    ]
    chrome_flags.extend(self.chrome_flags)

    content = "\n".join(chrome_flags)

    try:
      with open(CHROME_DEV_CONFIG_PATH, "w", encoding="utf-8") as f:
        f.write(content)
        f.flush()
        os.fsync(f.fileno())
      logging.info(f"Wrote Chrome configuration to {CHROME_DEV_CONFIG_PATH}")
      return True
    except IOError as e:
      raise IOError(f"Failed to write to {CHROME_DEV_CONFIG_PATH}: {e}") from e

  def restart_chrome_ui(self):
    """Restarts the Chrome UI to apply new configurations."""
    logging.info("Restarting Chrome UI...")
    try:
      subprocess.run(["restart", "ui"],
                     check=True,
                     capture_output=True,
                     text=True)
      logging.info("Chrome UI restarted successfully.")
    except (FileNotFoundError, subprocess.CalledProcessError) as e:
      raise RuntimeError(f"Error restarting UI: {e}") from e

  def cleanup(self):
    """Gracefully terminates the fake_dmserver process and cleans up."""
    logging.info("Cleaning up...")

    if self.dmserver_process and self.dmserver_process.poll() is None:
      logging.info(f"Terminating fake_dmserver "
                   f"(PID: {self.dmserver_process.pid})...")
      self.dmserver_process.terminate()
      try:
        self.dmserver_process.wait(timeout=5)
        logging.info("fake_dmserver terminated.")
      except subprocess.TimeoutExpired:
        logging.warning("fake_dmserver did not terminate gracefully, killing.")
        self.dmserver_process.kill()

    config_path = CHROME_DEV_CONFIG_PATH
    backup_prefix = config_path + ".original."

    if os.path.exists(config_path):
      os.remove(config_path)

    # Find the latest timestamped backup to restore
    backups = sorted([
        f for f in os.listdir("/etc")
        if f.startswith(os.path.basename(backup_prefix))
    ])
    if backups:
      latest_backup_path = os.path.join("/etc", backups[-1])
      try:
        os.rename(latest_backup_path, config_path)
        logging.info(
            f"Restored original {config_path} from {latest_backup_path}")
        self.restart_chrome_ui()
      except OSError as e:
        logging.error(
            f"Failed to restore original {config_path} from "
            f"{latest_backup_path}: {e}",
            exc_info=True)

    logging.info("Cleanup complete.")

  def run(self):
    """Main script execution."""

    if not os.path.exists(self.policy_file):
      raise FileNotFoundError(f"Input file not found at '{self.policy_file}'")

    if not os.path.exists(FAKE_DMSERVER_PATH):
      raise FileNotFoundError(
          f"fake_dmserver not found at {FAKE_DMSERVER_PATH}")

    os.makedirs(PERSISTENT_DATA_DIR, exist_ok=True)
    logging.info(f"Using persistent data directory: {PERSISTENT_DATA_DIR}")

    self.generate_policy_blob(self.policy_file, PERSISTENT_DATA_DIR)
    logging.info("Policy conversion complete.")

    read_fd, write_fd = os.pipe()

    policy_blob_path = os.path.join(PERSISTENT_DATA_DIR, "policy.json")
    client_state_path = os.path.join(PERSISTENT_DATA_DIR, "state.json")
    dmserver_args = [
        FAKE_DMSERVER_PATH,
        f'--policy-blob-path={policy_blob_path}',
        f'--client-state-path={client_state_path}',
        f"--startup-pipe={write_fd}",
    ]
    logging.info(f"Starting fake_dmserver: {' '.join(dmserver_args)}")
    # pylint: disable=consider-using-with
    self.dmserver_process = subprocess.Popen(
        dmserver_args,
        pass_fds=[write_fd],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    os.close(write_fd)  # Close the write end in the parent

    server_info_raw = ""
    try:
      with os.fdopen(read_fd) as pipe_reader:
        # Set a timeout for reading from the pipe
        signal.alarm(30)
        server_info_raw = pipe_reader.read()
        signal.alarm(0)  # Disable the alarm
    except TimeoutError:  # pylint: disable=broad-except-clause
      stdout, _ = self.dmserver_process.communicate()
      raise RuntimeError("Timed out waiting for fake_dmserver to start. "
                         f"fake_dmserver output:\n{stdout}") from None

    if not server_info_raw:
      stdout, _ = self.dmserver_process.communicate()
      raise RuntimeError(
          "fake_dmserver did not report its address. It may have crashed. "
          f"fake_dmserver output:\n{stdout}") from None

    server_info = json.loads(server_info_raw)
    ping_url = f"http://{server_info['host']}:{server_info['port']}/test/ping"

    logging.info(f"Waiting for fake_dmserver to respond at {ping_url}...")
    for _ in range(10):
      try:
        with urllib.request.urlopen(ping_url, timeout=2) as response:
          if response.status == 200:
            logging.info("fake_dmserver is responsive.")
            break
      except (urllib.error.URLError, ConnectionRefusedError):
        time.sleep(0.5)
    else:
      raise RuntimeError("fake_dmserver did not become responsive.")

    device_management_url = (
        f"http://{server_info['host']}:{server_info['port']}/device_management")
    self.configure_chrome_for_local_server(device_management_url)

    self.restart_chrome_ui()

    logging.info("fake_dmserver is running. Policies should now be applied.")
    logging.info("Check chrome://policy on your device.")
    logging.info("Press Ctrl+C to stop the server and clean up.")

    last_mtime = os.path.getmtime(self.policy_file)
    try:
      while True:
        try:
          current_mtime = os.path.getmtime(self.policy_file)
          if current_mtime > last_mtime:
            logging.info("Policy file changed. Re-generating policy blob...")
            self.generate_policy_blob(self.policy_file, PERSISTENT_DATA_DIR)
            logging.info("Policy blob re-generation complete.")
            last_mtime = current_mtime
          time.sleep(1)
        except FileNotFoundError:
          logging.warning(f"Policy file '{self.policy_file}' not found. "
                          "Will re-check in 1 second.")
          time.sleep(1)

    except Exception as e:
      raise RuntimeError(f"An unexpected error occurred: {e}") from e


def main():
  """Main script execution."""

  parser = argparse.ArgumentParser(
      description="Automates local policy testing on a ChromeOS device.\n\n"
      "This script handles generating policy blobs, starting fake_dmserver,\n"
      "configuring Chrome, and restarting the UI in a single command.",
      epilog="""For detailed usage instructions, including device setup,
policy file format, and advanced options, please refer to the README.md in
this directory.""",
      formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument(
      "policy_file",
      help="Path to a simple JSON file defining user and/or device "
      "policies.",
  )
  parser.add_argument(
      "--chrome-flags",
      action="append",
      help="Additional flags to pass to Chrome. Can be specified multiple "
      "times.",
  )
  args = parser.parse_args()

  orchestrator = Orchestrator(args.policy_file, args.chrome_flags)
  atexit.register(orchestrator.cleanup)
  signal.signal(signal.SIGINT, lambda sig, frame: sys.exit(0))
  signal.signal(signal.SIGTERM, lambda sig, frame: sys.exit(0))
  try:
    orchestrator.run()
  except (FileNotFoundError, ValueError, RuntimeError, IOError) as e:
    logging.critical(f"Error: {e}", exc_info=True)
    sys.exit(1)
  except Exception as e:
    logging.critical(f"An unexpected error occurred: {e}", exc_info=True)
    sys.exit(1)


if __name__ == "__main__":
  main()
