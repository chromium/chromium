# Local Policy Testing on the Desktop

This workflow is for users who want to run `fake_dmserver` for desktop Chrome on
Linux. You will use the generator script to create a policy blob, and then
manually start `fake_dmserver`.

### Step 1: Define Your Policies

Create a simple JSON file with the policies you want to test (e.g.,
`$HOME/dmserver_data/policies.json`).

### Step 2: Build Chrome

`blob_generator.py` depends on Python protobuf bindings generated during
Chrome's build process. You need a fresh build of Chrome before you can run
`blob_generator.py`:

```bash
autoninja -C out/Default chrome
```

### Step 3: Generate the Policy Blob

Use the `blob_generator.py` script to convert your simple JSON into the format
`fake_dmserver` understands. Pass the `--build-path` argument from step 2.

```bash
# Make sure the output directory exists
mkdir -p $HOME/dmserver_data

vpython3 components/policy/tools/fake_dmserver/blob_generator.py \
  --build-path=out/Default \
  --input-policies=$HOME/dmserver_data/policies.json \
  --output-blob=$HOME/dmserver_data/policy.json \
  --manual-map=components/policy/resources/templates/manual_device_policy_proto_map.yaml
```

### Step 4: Start `fake_dmserver`

Manually start the `fake_dmserver` on the device, pointing it to your generated
policy blob.

```bash
# Build the server binary
autoninja -C out/Default fake_dmserver

# Start the server on your desired port.
./out/Default/fake_dmserver \
    --policy-blob-path=$HOME/dmserver_data/policy.json \
    --client_state_path=$HOME/dmserver_data/state.json \
    --port=8080
```

### Step 5: Launch Chrome

When launching Chromium or Chrome, tell it to use your local server. For
Chromium builds, add `--enable-chrome-browser-cloud-management`:

```bash
# Replace the URL with the host and port from the previous step
./out/Default/chrome \
  --enable-chrome-browser-cloud-management \
  --device-management-url=http://127.0.0.1:8080/device_management"
```

## 2. How to Set Valid Policies (`policies.json`)

The `policies.json` file has a simple structure. The top-level keys are used to
configure the `fake_dmserver` itself, while the `user` and `device` sections
define the actual policies to be applied.

### Configuration Parameters

*   `user`: (Object) A dictionary where keys are user policy names and values
    are the policy values.
*   `machine-level-user`: (Object) A dictionary where keys are browser policy
    names and values are the policy values.
*   `machine-level-extension-install`: (Object) A dictionary where keys are
    "extension_id@version" extensions and values are objects:
    * `action`: (String enum) `"allow"` or `"block"`
    * `reasons`: (Array of String enums) elements can be `"blocked_category"`
      or `"risk_score"`
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
  },
  "machine-level-extension-install": {
    "abcdefghijklmnopabcdefghijklmnop@1.0.0": {
      "action": "block",
      "reasons": ["risk_score"]
    }
  },
  "use_universal_signing_keys": true
}
```

For full details on available policies and value formats, please refer to the
[Chrome Enterprise policies documentation]
(https://chromeenterprise.google.com/policies/).

## 3. Troubleshooting

### "Policy not found" Warnings
If you see a warning like `WARNING: Policy 'YourPolicyName' not found in
protobuf schema. Skipping.`, you may need to re-build `chrome` and re-run the
script.

This will update the policy definitions used by the tool to match the latest
version of Chrome.
