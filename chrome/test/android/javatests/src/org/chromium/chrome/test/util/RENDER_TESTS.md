# Render Tests

## Fixing a failing Render Test

### Failing on trybots

To investigate why a Render Test is failing on the trybots:

1. On the failed trybot run, locate and follow the `results_details` link under
the `chrome_public_test_apk` step to go to the **Suites Summary** page.
2. On the **Suites Summary** page, follow the link to the test suite that is
failing.
3. On the **Test Results of Suite** page, follow the links in the **log** column
corresponding to the renders mentioned in the failure stack trace. The links
will be of the form `<test class>.<render id>.<device details>.png`.

Now you will see a **Render Results** page, showing:

* Some useful links.
* The **Failure** image, what the rendered Views look like on the test device.
* The **Golden** image, what the rendered Views should look like, according to
the golden files checked into the repository.
* A **Diff** image to help compare.

At this point, decide whether the UI change was intentional. If it was, follow
the steps below to update the golden files stored in the repository. If not, go
and fix your code! If there's some other error or flakiness, file a bug to
`peconn@chromium.org`.

1. Use the `Link to Golden` link to determine where in the repository the golden
was stored.
2. Right click on the `Download Failure Image` link to save the failure image in
the appropriate place in your local repository.
3. Run the script
`//chrome/test/data/android/manage_render_test_goldens.py upload` to upload the
new goldens to Google Storage and update the hashes used to download them.
4. Reupload the CL and run it through the trybots again.

When putting a change up for review that changes goldens, please include links
to the results_details/Render Results pages that you grabbed the new goldens
from. This will help reviewers confirm that the changes to the goldens are
acceptable.

If you add a new device/SDK combination that you expect golden images for, be
sure to add it to `ALLOWED_DEVICE_SDK_COMBINATIONS` in
`//chrome/test/data/android/manage_render_test_goldens.py`, otherwise the
goldens for it will not be uploaded.

### Failing locally

Follow the steps in [*Running the tests locally*](#running-the-tests-locally)
below to generate renders.

You can rename the renders as appropriate and move them to the correct place in
the repository, or you can open the locally generated **Render Results** pages
and follow steps 2-3 in the second part of the
[*Failing on trybots*](#failing-on-trybots) section.


## Writing a new Render Test

### Writing the test

To write a new test, start with the example in the javadoc for
[RenderTestRule](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/util/RenderTestRule.java).

### Running the tests locally

When running instrumentation tests locally, pass the `--local-output` option to
the test runner to generate a folder in your output directory (in the example
`out/Debug`) looking like `TEST_RESULTS_2017_11_09T13_50_49` containing the
failed renders, eg:

```
./out/Debug/bin/run_chrome_public_test_apk -A Feature=RenderTest --local-output
```

The golden images should be downloaded as part of the `gclient sync` process,
but if there appear to be goldens missing that should be there, try running
`//chrome/test/data/android/manage_render_test_goldens.py download` to ensure
that the downloaded goldens are current for the git revision.

### Generating golden images locally

New golden images may be downloaded from the trybots or retrieved locally. This
section elaborates how to do the latter.

You should always create your reference images on the same device type as the
one running the tests. This is because each device/API version may produce a
slightly different image, eg. due to different screen dimensions, DPI setting,
or styling used across OS versions. This is also why each golden image name
includes the device name and API version.

When running a test with no goldens on the correct device, your tests should
fail with an exception:

```
RenderTest Goldens missing for: <reference>. See RENDER_TESTS.md for how to fix this failure.
```

You will be able to find the images the device captured on the device's SD card.

```
adb -d shell ls /sdcard/chromium_tests_root/chrome/test/data/android/render_tests/failures
```

## Implementation Details

### Supported devices

How a View is rendered depends on both the device model and the version of
Android it is running. We only want to maintain golden files for model/SDK pairs
that occur on the trybots, otherwise the golden files will get out of date as
changes occur and render tests will either fail on the Testers with no warning,
or be useless.

Currently, `chrome_public_test_apk` is only run on Nexus 5s running Android
Lollipop, so that is the only model/sdk combination for which we store goldens.
There [is work](https://crbug.com/731759) to expand this to include Nexus 5Xs
running Marshmallow as well.

### Sanitizing Views

Certain features lead to flaky tests, for example any sort of animation we don't
take into account while writing the tests. To help deal with this, you can use
`RenderTestRule.sanitize` to modify the View hierarchy and remove some of the
more troublesome attributes (for example, it disables the blinking cursor in
`EditText`s).

