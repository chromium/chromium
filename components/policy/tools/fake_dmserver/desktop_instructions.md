# Local Policy Testing on the Desktop

This workflow is for users who want to run `fake_dmserver` for desktop Chrome on
Linux. You will use the generator script to create a policy blob, and then
manually start `fake_dmserver`.

### Step 1: Define Your Policies

Create a simple JSON file with the policies you want to test (e.g.,
`$HOME/dmserver_data/policies.json`).

> **Important**: Desktop Chrome and Chromium validate cloud policy signatures
> against Google's production verification key compiled into the browser binary.
> `fake_dmserver` includes pre-computed production key signatures for three
> domains only:
> - `example.com`
> - `chromepolicytest.com`
> - `managedchrome.com`
>
> You must set `policy_user` to an address using one of these three domains
> (for example, `alice@example.com`). Other domains will fail validation or
> cause `fake_dmserver` to return an HTTP 400 error.

Here is a minimal example for testing both machine-level and user-level
policies.

```json
{
  "policy_user": "alice@example.com",
  "machine": {
    "AllowDinosaurEasterEgg": true,
    "ExtensionInstallForcelist": [
      "eimadpbcbfnmbkopoojfekhnkhdbieeh;https://clients2.google.com/service/update2/crx"
    ]
  },
  "user": {
    "HomepageLocation": "http://example.com/"
  }
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

Optional arguments:
- `--help`, `-h`: Show help message.
- `--port=...`: Set the server port (defaults to 6112; 0 for random port).
- `--fcm-port=...`: Set the mock FCM push notification port (defaults to 6113;
  0 for random port).
- `--no-fcm`: Disable the mock FCM push notification server.

### Step 3: Launch Chrome

When launching Chromium or Chrome, tell it to use your local server. Use
`--use-fake-dmserver` to configure default DMServer and real-time reporting
URLs, and automatically enable cloud management:

```bash
./out/Default/chrome --use-fake-dmserver
```

The `--use-fake-dmserver` switch is available only when the
`allow_fake_dmserver` GN arg is true. The arg is true by default in Chromium
builds on Linux, and false in Google Chrome builds. To use the switch in a local
Google Chrome build, add this line to `args.gn`:

```
allow_fake_dmserver = true
```

To enroll the browser in Chrome Browser Cloud Management (CBCM) without
configuring an OS-level `CloudManagementEnrollmentToken`, use:

```bash
./out/Default/chrome --use-fake-dmserver=always-enroll
```

This supplies a dummy enrollment token when no token is provided by the
operating system, allowing machine-level enrollment to proceed immediately.

Alternatively, or for builds where `allow_fake_dmserver` is false, specify the
parameters manually:

```bash
# Replace the URL with the host and port from the previous step
./out/Default/chrome \
  --enable-chrome-browser-cloud-management \
  --device-management-url=http://127.0.0.1:6112/device_management \
  --realtime-reporting-url=http://127.0.0.1:6112/realtime_reporting \
  --gcm-mcs-endpoint=http://127.0.0.1:6113 \
  --gcm-checkin-url=http://127.0.0.1:6112/gcm_checkin \
  --gcm-registration-url=http://127.0.0.1:6112/gcm_register
```

Note that some features only work with `--use-fake-dmserver`.

### Step 4: Test Your Feature

Use server logs for debugging. `policies.json` will auto-reload without
needing to restart the server. When Chrome is launched with the GCM flags
above (`--gcm-mcs-endpoint`, `--gcm-checkin-url`, `--gcm-registration-url`),
changes to `policies.json` also push invalidations to Chrome automatically;
otherwise (such as with `--use-fake-dmserver` alone), click "Reload policies"
on `chrome://policy` to fetch updated policies.

Browser reports, profile reports, and real-time reporting events are saved in
`$HOME/dmserver_data`.

### Step 5: Web Administration Dashboard

`fake_dmserver` provides an interactive web dashboard at
`http://127.0.0.1:6112/admin`.

Supported features:
* View registered clients.
* View browser reports, profile reports, and real-time reporting events.
* Edit policies live with immediate push notification updates.
* Send remote commands and view execution results.

### Step 6: REST API

`fake_dmserver` also provides a REST API for automated testing and scripting.
See [rest_api.md](rest_api.md) for endpoint details and examples.

## 2. How to Set Valid Policies (`policies.json`)

The `policies.json` file has a simple structure. The top-level keys are used to
configure the `fake_dmserver` itself, while the `user` and `device` sections
define the actual policies to be applied.

### Configuration Parameters

*   `machine` or `user`: (Object) A dictionary where keys are user policy names
    and values are the mandatory policy values.
*   `recommended_machine` or `recommended_user`: (Object) A dictionary where
    keys are user policy names and values are the recommended policy values.
*   `machine-extension-install`, `user-extension-install`: (Object) A dictionary
    where keys are "extension_id@version" extensions and values are objects:
    * `action`: (String enum) `"allow"` or `"block"`
    * `reasons`: (Array of String enums) elements can be `"blocked_category"`
      or `"risk_score"`
*   `machine-extensions`, `user-extensions`: (Object) A dictionary where keys
    are 32-character extension IDs (`[a-p]{32}`) and values are dictionaries
    of policy names to values for `chrome.storage.managed`. Values can be raw
    values (e.g., `{"PolicyName": "value"}`) or explicit dictionaries with
    a `"Value"` key (e.g., `{"PolicyName": {"Value": "val"}}`).
*   `policy_user`: (String) The user ID (e.g., email address) to embed in
    policy responses. On desktop, this must use one of the supported signature
    domains (`example.com`, `chromepolicytest.com`, or `managedchrome.com`), and
    defaults to `username@example.com` if omitted. When using
    `--use-fake-dmserver`, you can sign into the browser with any managed
    account without matching `policy_user`.
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
    universal test key to sign policy blobs with a wildcard signature. Do **not**
    set this for local desktop Chrome or Chromium testing; standard browser
    binaries validate against Google's production verification key and will reject
    test signatures. This option is intended only for ChromeOS test images (with
    `--disable-policy-key-verification`) or automated test suites (with
    `--policy-verification-key`).
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
    "AllowDinosaurEasterEgg": true
  },
  "recommended_user": {
    "ShowHomeButton": false
  },
  "machine": {
    "ShowHomeButton": true,
    "HomepageLocation": "https://www.google.com",
    "RestoreOnStartup": 1,
    "URLBlocklist": [
      "example.com",
      "unwanted-site.org"
    ]
  },
  "recommended_machine": {
    "BookmarkBarEnabled": true
  },
  "machine-extensions": {
    "abcdefghijklmnopabcdefghijklmnop": {
      "server_url": "https://sync.example.com",
      "sync_frequency": {
        "Value": 300,
        "Level": "Recommended"
      }
    }
  },
  "machine-extension-install": {
    "abcdefghijklmnopabcdefghijklmnop@1.0.0": {
      "action": "block",
      "reasons": ["risk_score"]
    }
  }
}
```

For full details on available policies and value formats, please refer to the
[Chrome Enterprise policies documentation]
(https://chromeenterprise.google.com/policies/).
