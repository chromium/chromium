# Local Policy Testing on the Desktop

This workflow is for users who want to run `fake_dmserver` for desktop Chrome on
Linux. You will use the generator script to create a policy blob, and then
manually start `fake_dmserver`.

### Step 1: Define Your Policies

Create a simple JSON file with the policies you want to test (e.g.,
`$HOME/dmserver_data/policies.json`).

Here is a minimal example, where you log into Chromium as alice@acme.com and
want both machine-level and user-level policies.

```
{
  "policy_user": "alice@acme.com",
  "machine": {
    "AllowDinosaurEasterEgg": true,
  },
  "user": {
    "HomepageLocation": "http://example.com/",
  },
}
```

You should put this file somewhere outside of `chromium/src`, so you don't
accidentally commit or upload them to Gerrit.

More examples of JSON files are at the end of this document.

### Step 2: Start `fake_dmserver`

Manually start the `fake_dmserver` on the device, pointing it to the file from
step 1.

```bash
# Build the server binary first.
autoninja -C out/Default fake_dmserver

# Start the server.
out/Default/fake_dmserver --policy-path=$HOME/dmserver_data/policies.json
```

This will create a `state.json` file in the same directory as `policies.json`,
for `fake_dmserver` to save/load its internal state.

Optionally arguments:
- `--port=...` to set the server port. Defaults to 6112.

### Step 3: Launch Chrome

When launching Chromium or Chrome, tell it to use your local server. For
Chromium builds, add `--enable-chrome-browser-cloud-management`:

```bash
# Replace the URL with the host and port from the previous step
./out/Default/chrome \
  --enable-chrome-browser-cloud-management \
  --device-management-url=http://127.0.0.1:6112/device_management"
```

## 2. How to Set Valid Policies (`policies.json`)

The `policies.json` file has a simple structure. The top-level keys are used to
configure the `fake_dmserver` itself, while the `user` and `device` sections
define the actual policies to be applied.

### Configuration Parameters

*   `machine` or `user`: (Object) A dictionary where keys are user policy names
    and values are the policy values.
*   `machine-extension-install`, `user-extension-install`: (Object) A dictionary
    where keys are "extension_id@version" extensions and values are objects:
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

More complete example showcasing some of `fake_dmserver`'s features:

```json
{
  "policy_user": "alice@example.com",
  "managed_users": ["alice@example.com", "bob@example.com"],
  "user": {
    "AllowDinosaurEasterEgg": true,
  },
  "machine": {
    "ShowHomeButton": true,
    "HomepageLocation": "https://www.google.com",
    "RestoreOnStartup": 1,
    "URLBlocklist": [
      "example.com",
      "unwanted-site.org"
    ],
  },
  "machine-extension-install": {
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
