# Assistant Browser Tests

## Introduction

This documentation concerns all Assistant browser/integration test which use
`AssistantTestMixin` or any derived class.

All these tests run the real LibAssistant code, and use a
[FakeS3Server](go/fake-s3-for-libassistant) to intercept and replay the
communication between LibAssistant and the cloud.  This allows these tests to
run in the CQ, without requiring any network connectivity.

## Adding a new test

To add a new integration test, your test fixture must inherit from
`MixinBasedInProcessBrowserTest` and must contain `AssistantTestMixin` as a
private variable:

    #include "chrome/browser/ui/ash/assistant/assistant_test_mixin.h"
    #include "chrome/test/base/mixin_based_in_process_browser_test.h"

    class MyBrowserTest : public MixinBasedInProcessBrowserTest {
     public:
      MyBrowserTest() = default;
      ~MyBrowserTest() override = default;

      AssistantTestMixin* tester() { return &tester_; }

     private:
      AssistantTestMixin tester_{&mixin_host_, this,
                                 embedded_test_server(),
                                 FakeS3Mode::kReplay};

    };

Tests can use API provided by `AssistantTextMixin` to interact with the
Assistant UI.  Look in `chrome/browser/ui/ash/assistant/assistant_test_mixin.h`
to see all the available methods.

Each test *must* start by starting the Assistant service and waiting for it to
signal it's ready.
That is done by calling `tester()->StartAssistantAndWaitForReady()`.

    IN_PROC_BROWSER_TEST_F(MyBrowserTest, MyVeryFirstTest) {
      tester()->StartAssistantAndWaitForReady();

      // Your test code comes here.
    }

## Using the fake S3 server

The fake S3 server can run in 3 different modes, all of which you'll need to
use while adding new tests.

You'll start by using the [_proxy_ mode](#in-proxy-mode) while developing a new
test; This simply forwards all requests to a real S3 server.

When you're ready to commit you'll switch to [_record_ mode](#in-record-mode)
in order to record the interaction between LibAssistant and the S3 server.

Afterwards you'll switch to [_replay_ mode](#in-replay-mode); In this mode the
fake S3 server will replay the recorded interactions. This allows the CQ to run
the tests without once requiring network connectivity to contact the real S3
server.

## Running the tests

### In proxy mode

In this mode the fake S3 server works as a simple proxy, forwarding all
LibAssistant requests to the real S3 server (and proxying the responses back).

As such, you will have to:

1. Tell the fake S3 server to use this mode.
2. Generate a real Gaia authentication token for the communication with the real S3 server.

The first part can be achieved by passing `|FakeS3Mode::kProxy|` as the final
argument to the constructor of the `|AssistantTextMixin|` instance in your test
fixture.

    // in your test fixture
    private:
      AssistantTestMixin tester_{&mixin_host_, this, embedded_test_server(),
                                 FakeS3Mode::kProxy};

The second part is achieved by running your tests through the wrapper script
`chromeos/assistant/internal/test_support/run_with_access_token.py`.

Start by opening a terminal in your `chromium/src` directory and then:

    # First compile the browser_tests target
    autoninja -C out/Default browser_tests
    # Then run it (change the --gtest_filter args to the filter you need)
    chromeos/assistant/internal/test_support/run_with_access_token.py out/Default/browser_tests --gtest_filter="AssistantBrowserTest.*"

Alternatively you can also store the Gaia token you'd like to use in the environmental variable `TOKEN`:

    export TOKEN="the.gaia.token.string"
    out/Default/browser_tests --gtest_filter="AssistantBrowserTest.*"

### In record mode

In this mode the fake S3 server will again work as a proxy, but it will also
store a recording of all the communications in a file called

    chromeos/assistant/internal/test_data/<testfixture_testname>.fake_s3.proto

A separate file is created for each test.

You will need to run once in *record* mode when your test is ready to be
committed.

To do this, use the same steps described [above](#in-proxy-mode) but replace
`|FakeS3Mode:kProxy|` with `|FakeS3Mode::kRecord|`.

Note that these `.fake_s3.proto` files will need to be committed before you can
commit your changes. See [here](#committing-your-tests) for more information.

### In replay mode

In this mode all S3 requests are handled by replaying the responses stored
while running in _record_ mode.

As this no longer contacts the real S3 server it does not need a Gaia token,
so you no longer needs to use the `run_with_access_token.py` script.

All you need to do is pass `|FakeS3Mode::kReplay|` as the final argument
to the constructor of the `|AssistantTextMixin|` instance in your test fixture.

    // in your test fixture
    private:
      AssistantTestMixin tester_{&mixin_host_, this, embedded_test_server(),
                                 FakeS3Mode::kReplay};

This is the mode the CQ runs in, and as such you must **make sure you've enabled this mode before pushing your CL!**

## Removing/Renaming a test

When a test is removed you should take care to remove the stale

    chromeos/assistant/internal/test_data/<testfixture_testname>.fake_s3.proto

file.

Conversively, when a test is renamed you should remove the old `.fake_s3.proto`
file and use [_record_ mode](#in-record-mode) to generate a new proto file.

## Committing your tests

Committing the tests has to be done in multiple steps:

1. Create an internal CL to add the new files in
   `chromeos/assistant/internal/test_data`.
2. Uprev this CL.
3. Create a CL for your browsertest changes
4. (Optionally): If you deleted tests, create an internal CL to remove their `.fake_s3.proto` files from `chromeos/assistant/internal/test_data`.

