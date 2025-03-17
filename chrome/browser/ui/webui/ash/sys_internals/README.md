# sys-internals WebUI

The `chrome://sys-internals` URL provides an overview of system internals in
ChromeOS, such as CPU and memory usage.

## Related Code

### Backend

-   The backend code is located in this folder and includes the main WebUI
    controller for the page.
-   It also contains the handler of the `getSysInfo` message, which collects
    various system metrics.

### Frontend

-   The frontend code resides in
    [chrome/browser/resources/chromeos/sys\_internals/](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/chromeos/sys_internals/).
-   It handles the visualization logic using HTML, JavaScript, and CSS.
-   It communicates with the backend through the `chrome.send()` RPC mechanism.

### Tests

-   Test code is located in
    [chrome/test/data/webui/chromeos/sys\_internals/](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/data/webui/chromeos/sys_internals/).
-   These tests primarily validate the frontend code.

## Workflow

You can use the usual
[Simple Chrome](https://www.chromium.org/chromium-os/developer-library/guides/development/simple-chrome-workflow/)
workflow for development.

### Tests

WebUI testing is performed on the host as part of `browser_tests`. Refer to
[Testing WebUI Pages](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/webui/testing_webui.md)
for detailed instructions.

*** note

Since this is intended to run on the host, the `args.gn` **SHOULD NOT**
include ChromeOS board-specific import statements, such as:

```gn
import("//build/args/chromeos/hatch.gni")
```

Including such statements may cause the following error when executing the test
binary:

```
error while loading shared libraries: libsync.so: cannot open shared object file: No such file or directory
```

***

#### Example Commands for Running Tests Locally

1.  Prepare the build output folder:

    ```shell
    mkdir -p out/cros_browser_test
    ```

2.  Generate GN args:

    ```shell
    gn gen out/cros_browser_test --args='target_os="chromeos" use_remoteexec=true use_siso=true is_chrome_branded=true is_component_build=false'
    ```

    The above assumes you are using a distributed compiler service to speed up
    compilation. Adjust these arguments as needed.

3.  Build the tests:

    ```shell
    autoninja -C out/cros_browser_test browser_tests
    ```

4.  Run the tests:

    ```shell
    ./testing/run_with_dummy_home.py \
      testing/xvfb.py \
      out/cros_browser_test/browser_tests \
      --gtest_filter='SysInternalsBrowserTest.*'
    ```

    Be patient. This process may take several minutes.
