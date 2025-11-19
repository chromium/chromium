# ChromeOS Local Policy Testing Tools

This document describes the tools and workflows for applying local policies to a
ChromeOS test device using `fake_dmserver`.

There are two primary ways to use these tools:
1.  **[Automated Usage (Recommended)](#2_automated-usage-recommended):**
    A simple, one-command process using the `orchestrator.py` script.
2.  **[Manual Usage (Advanced)](#3_manual-usage-advanced):** A step-by-step
    process that gives you more control by using the `blob_generator.py` script
    directly.

## 1. Prerequisites (for both workflows)

### One-Time Device Setup
Before you begin, you must disable root filesystem verification on your test
device. This allows you to write the necessary configuration files.

**Run these commands in a terminal on your DUT:**
```bash
sudo /usr/share/vboot/bin/make_dev_ssd.sh --remove_rootfs_verification \
  --force && sudo reboot
```

After the device reboots, remount the filesystem as read-write for your current
session:
```bash
sudo mount -o remount,rw /
```

## 2. Automated Usage (Recommended)

This workflow uses the `orchestrator.py` script to automate the entire process
of generating policies, starting the server, configuring Chrome, and restarting
the UI.

### Step 1: Define Your Policies
Create a simple JSON file with the policies you want to test. A good location is
`/usr/local/tmp/policies.json`.

*(See the section "[How to Set Valid Policies](#4_how-to-set-valid-policies)"
below for details on the file format.)*

### Step 2: Run the Orchestrator
Execute the orchestrator script with the path to your policy file. It will
handle everything else.

**Run this command on the DUT:**
```bash
python3 /usr/local/share/policy-test-tool/orchestrator.py \
  /usr/local/tmp/policies.json
```
The script will start the server and restart the UI. You can then log in and
verify your policies at `chrome://policy`. The script will continue to watch
your `policies.json` file for changes.

## 3. Manual Usage (Advanced)

This workflow is for users who want more control over the process. You will use
the generator script to create the policy blob and then manually perform the
necessary system actions.

### Step 1: Define Your Policies
Create a simple JSON file with the policies you want to test (e.g.,
`/usr/local/tmp/policies.json`).

### Step 2: Generate the Policy Blob
Use the `blob_generator.py` script to convert your simple JSON into the format
`fake_dmserver` understands.

**Run this command on the DUT:**
```bash
# Make sure the output directory exists
mkdir -p /var/tmp/dmserver_data

python3 /usr/local/share/policy-test-tool/blob_generator.py \
  --input-policies=/usr/local/tmp/policies.json \
  --output-blob=/var/tmp/dmserver_data/policy.json \
  --manual-map=/usr/local/share/policy-test-tool/manual_device_policy_proto_map.yaml
```

### Step 3: Start `fake_dmserver`
Manually start the `fake_dmserver` on the device, pointing it to your generated
policy blob. This command typically requires root permissions.

**Run this command on the DUT:**
```bash
# Start the server and take note of the host and port it prints out
/usr/local/libexec/chrome-binary-tests/fake_dmserver \
    --policy-blob-path=/var/tmp/dmserver_data/policy.json
```
The server will output its address, for example:
`{"host": "127.0.0.1", "port": 8080}`.

### Step 4: Configure Chrome
Write a configuration file to tell Chrome to use your local server.

**Run this command on the DUT (you will need to be root):**
```bash
# Replace the URL with the host and port from the previous step
    echo "--device-management-url=http://127.0.0.1:8080/device_management" \
  >> /etc/chrome_dev.conf```

### Step 5: Restart the UI
Restart the Chrome UI to force it to reload the configuration and fetch the new
policies.

**Run this command on the DUT (you will need to be root):**
```bash
restart ui
```

## 4. How to Set Valid Policies (`policies.json`)
*(This section applies to both workflows)*

The `policies.json` file has a simple structure. The top-level keys are used to
configure the `fake_dmserver` itself, while the `user` and `device` sections
define the actual policies to be applied.

You can create this file manually, or you can generate it by following the
steps in the [converter script](#6_converting-existing-policies-from-chrome-policy-dump)
described in the next section.

### Configuration Parameters

*   `user`: (Object) A dictionary where keys are user policy names and values
    are the policy values.
*   `device`: (Object) A dictionary where keys are device policy names and
    values are the policy values.
*   `policy_user`: (String) The user ID (e.g., email address) to embed in
    policy responses. This must match the user on the ChromeOS device, or the
    policy will be rejected.
*   `managed_users`: (Array of Strings) A list of users that the server should
    treat as managed. Use `"*"` to treat all users as managed.
*   `device_affiliation_ids`: (Array of Strings) A list of affiliation IDs for
    the device.
*   `user_affiliation_ids`: (Array of Strings) A list of affiliation IDs for
    the user.
*   `directory_api_id`: (String) The Directory API ID of the device.
*   `robot_api_auth_code`: (String) The authorization code for the robot
    account, used during enterprise enrollment.
*   `allow_set_device_attributes`: (Boolean) If `true`, allows device
    attributes to be set by the client. Defaults to `true`.
*   `use_universal_signing_keys`: (Boolean) If `true`, the server will use a
    universal test key to sign policy blobs. This is recommended for most local
    testing scenarios.
*   `current_key_index`: (Integer) The index of the signing key to use.
*   `request_errors`: (Object) A dictionary to simulate server errors. The keys
    are request types (e.g., `"register"`, `"policy"`) and the values are HTTP
    error codes (e.g., `500`).
*   `initial_enrollment_state`: (Object) A dictionary defining the initial
    state for zero-touch enrollment.

### Example `policies.json`

```json
{
  "policy_user": "<user>@gmail.com",
  "managed_users": ["*"],
  "user": {
    "AllowDinosaurEasterEgg": true,
    "ShowHomeButton": true,
    "HomepageLocation": "https://www.google.com",
    "RestoreOnStartup": 1,
    "URLBlocklist": [
      "example.com",
      "unwanted-site.org"
    ],
    "ManagedBookmarks": "[{\"name\":\"Google Search\",\"url\":\"https://www.google.com\"},{\"name\":\"Internal Tools\",\"children\":[{\"name\":\"Moma\",\"url\":\"https://moma.corp.google.com\"},{\"name\":\"Critique\",\"url\":\"https://critique.corp.google.com\"}]},{\"name\":\"News\",\"url\":\"https://news.google.com\"}]"
  },
  "device": {
    "DeviceGuestModeEnabled": true,
    "DevicePolicyRefreshRate": 3600000,
    "DeviceLocalAccounts": [
      {
        "account_id": "<user>@gmail.com",
        "type": "KIOSK_IWA",
        "isolated_kiosk_app": {
          "update_manifest_url": "https://github.com/chromeos/iwa-sink/releases/latest/download/update.json",
          "web_bundle_id": "aiv4bxauvcu3zvbu6r5yynoh4atkzqqaoeof5mwz54b4zfywcrjuoaacai"
        }
      }
    ],
    "DeviceLocalAccountAutoLoginId": "<user>@gmail.com"
  },
  "use_universal_signing_keys": true
}
```
For full details on available policies and value formats, please refer to the
[Chrome Enterprise policies documentation]
(https://chromeenterprise.google.com/policies/).

## 5. Custom Chrome Flags

The `orchestrator.py` script allows you to pass additional command-line flags
directly to Chrome using the `--chrome-flags` argument. This is useful for
enabling experimental features, debugging, or overriding default Chrome
behavior.

You can specify this argument multiple times for different flags:

```bash
python3 /usr/local/share/policy-test-tool/orchestrator.py \
  /usr/local/tmp/policies.json \
  --chrome-flags="--remote-debugging-port=9222" \
  --chrome-flags="--enable-features=IsolatedWebAppDevMode"
```

## 6. Converting Existing Policies from `chrome://policy` dump

If you want to replicate the policy state of an existing device, you can use the
`policy_dump_converter.py` script.

### Understanding the Format Difference
The JSON file you export from `chrome://policy` is **different** from the
simplified `policies.json` format required by these tools. The dump from
`chrome://policy` is a detailed report of the current state, including metadata
like policy scope, source, and whether it was successfully applied.

The `policies.json` format, on the other hand, is a simple, clean definition of
the policies you *want* to set. The converter script bridges this gap by
extracting only the necessary information (name, value, and scope) and
structuring it correctly for the `fake_dmserver`.

### How to Use the Converter

**Step 1: Export Policies from Chrome**
Navigate to `chrome://policy` on the device you want to copy policies from,
click "Export to JSON", and save the file.

**Step 2: Run the Converter Script**
Run the converter script on the DUT, providing the exported JSON, a path for the
new `policies.json` file, and the managed user's email.

```bash
python3 /usr/local/share/policy-test-tool/policy_dump_converter.py \
  --input-dump=/path/to/your/policy_export.json \
  --output-policies=/usr/local/tmp/policies.json \
  --policy-user=user@managedchrome.com
```

**Step 3: Use the Generated File**
The newly created `/usr/local/tmp/policies.json` can now be used with the
`orchestrator.py` script as described in the "[Automated
Usage](#2_automated-usage-recommended)" section.

## 7. Troubleshooting

### "Policy not found" Warnings
If you see a warning like `WARNING: Policy 'YourPolicyName' not found in
protobuf schema. Skipping.`, it means the version of the `policy-test-tool` on
your device is older than the version of Chrome running on it.

**To fix this:**
1.  Update your Chromium source code to the latest version.
2.  Rebuild and redeploy the `policy-test-tool` package to your DUT.

This will update the policy definitions used by the tool to match the latest
version of Chrome.

## 8. Testing a New (Unlanded) Policy

If you are developing a new policy that has not yet been landed in the Chromium
source tree, the testing tools will not recognize it. To test it, you need to
manually update the policy definitions in your local checkout before building
the package.

### Step 1: Add Your Policy to `policy_templates.json`

This file is the source of truth for all Chrome policies. You need to add a
definition for your new policy here.

1.  **Edit the file:**
    `chromium/src/components/policy/resources/policy_templates.json`
2.  **Add your policy definition** to the `policy_definitions` list. Ensure the
    `name`, `type`, `schema`, and `supported_on` fields are correct.

    ```json
    {
      'name': 'MyNewAwesomePolicy',
      'type': 'boolean',
      'schema': { 'type': 'boolean' },
      'supported_on': ['chrome_os:145-'],
      'features': { 'dynamic_refresh': true, 'per_profile': true },
      'example_value': true,
      'caption': 'Enable My New Awesome Policy',
      'desc': 'When this policy is enabled, it activates my new awesome feature.'
    }
    ```

### Step 2: Regenerate the Protobuf Schema

After updating the policy definitions, you must regenerate the
`chrome_settings.proto` file, which is used to build the Python modules.

*   **Run the generator script** from your `chromium/src` directory:
    ```bash
    python3 components/policy/tools/generate_policy_source.py \
      --chrome-settings-protobuf=components/policy/proto/chrome_settings.proto \
      --policy-templates-file=components/policy/resources/policy_templates.json \
      --target-platform="chrome_os"
    ```

### Step 3: Rebuild and Deploy the Package

Now that your local source code includes the new policy definition, you can
rebuild and deploy the `policy-test-tool`.

1.  **Build the package:** The build process will use your updated `.proto`
    file.
    ```bash
    emerge-<board> chromeos-base/policy-test-tool
    ```
2.  **Deploy to your DUT:**
    ```bash
    cros deploy <DUT_IP> chromeos-base/policy-test-tool
    ```

### Step 4: Test Your Policy on the DUT

You can now add your new policy to your `policies.json` file and use the
orchestrator script.

1.  **Edit `/usr/local/tmp/policies.json` on the DUT** and add your new policy
    to the `user` or `device` section.
2.  **Run the orchestrator:**
    ```bash
    python3 /usr/local/share/policy-test-tool/orchestrator.py \
      /usr/local/tmp/policies.json
    ```

The tool will now recognize and apply your new policy.

## 9. Local Development Workflow

If you are making changes to the `policy-test-tool` scripts themselves (e.g.,
`orchestrator.py`, `blob_generator.py`, `policy_dump_converter.py`), follow
these steps to test your changes on the DUT:

### Step 1: Start `cros_workon`

On your development machine, enable `cros_workon` for the `policy-test-tool`
package. This allows you to make changes to the source code and have them
reflected in the build.

```bash
cros_workon --board=<board> start chromeos-base/policy-test-tool
```

### Step 2: Make Your Changes

Edit the Python scripts in your local ChromiumOS checkout
(`chromiumos/src/chromium/src/components/policy/tools/fake_dmserver/orchestrator.py`,
`chromiumos/src/chromium/src/components/policy/tools/fake_dmserver/blob_generator.py`,
`chromiumos/src/chromium/src/components/policy/tools/fake_dmserver/policy_dump_converter.py`)
or directly on the DUT at `/usr/local/share/policy-test-tool/`.

### Step 3: Rebuild and Deploy the Package

After making your changes, rebuild and deploy the `policy-test-tool` package to
your DUT. This will install your modified scripts on the device.

1.  **Build the package:**
    ```bash
    emerge-<board> chromeos-base/policy-test-tool
    ```
2.  **Deploy to your DUT:**
    ```bash
    cros deploy <DUT_IP> chromeos-base/policy-test-tool
    ```

### Step 4: Test Your Changes on the DUT

Once deployed, your changes to the `policy-test-tool` scripts will be active on
the DUT. You can then run the orchestrator or other scripts as usual to test
your modifications.

```bash
python3 /usr/local/share/policy-test-tool/orchestrator.py \
  /usr/local/tmp/policies.json
```
