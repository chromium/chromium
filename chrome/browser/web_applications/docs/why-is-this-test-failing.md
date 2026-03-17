# Why are these WebAppIntegration_* tests failing?

The integration tests for the WebAppProvider system test as much of the system as possible from user actions, and it can sometimes be difficult to categorize why the failure is occurring.

## What do I do?

1. Is this a flaky test that is showing up on the flakiness dashboard with a score > 200? It is reasonable to disable this test. See the [flakiness](#flakiness) section, and the [disabling tests section](#disabling-tests) about how to disable the test.
2. Are a huge number of tests suddenly failing? This is probably because behavior was changed that broke the critical user journeys that these tests come from. Either:
    1. A regression was introduced in a CL that needs to be reverted. If this is the case, that CL should probably to be found and reverted.
    2. The current CL changed the way the system works which conflicts with the critical user journeys. If this is the case, the critical user journeys need to be updated. Please contact the team at  pwa-dev@chromium.org and/or post on #pwas on Chromium Slack, as we can help update the framework & guide this process.

### Disabling tests

Tests can be disabled in the same manner that other integration/browser tests are disabled, using macros. See [on disabling tests](/docs/testing/on_disabling_tests.md) for more information.

## Why are they failing?

### System behavior is changing

Tests are expected to fail when the WebAppProvider behavior or user actions are changed. To fix this, the critical user journeys need to be updated and the tests regenerated to modify the expected behavior. See the [docs](integration-testing-framework.md) or the ["How to create WebappIntegration Tests"](how-to-create-webapp-integration-tests.md) page for more info. Please contact the team at  pwa-dev@chromium.org and/or post on #pwas on Chromium Slack, as we can help update the framework & guide this process.

This failure **should** occur in trybots on the change, and not the CQ, although we do have a history of having browser_tests turned off for Mac trybots and sometimes things are only caught on the CQ.

### Flakiness

Due to how much of Chrome this is testing, a baseline of flakes is unfortunately somewhat common. But if they occur frequently, they often point to real bugs that are flaky in production as well.

Disabling the browsertest for flakiness resembles disabling any other browsertest, by having a `DISABLED_` prefix. When disabling, please try to target it to the affected platform.

It can be useful to check the [bugs](https://bugs.chromium.org/p/chromium/issues/list?q=component%3APlatform%3EWebAppProvider%3EIntegrationTesting&can=2) in the component and the test log to see if there is already a bug open for the given problem or test. If specific to the web app system, then it is useful to add new logs to the bug.

Sometimes global problems affect the tests (and all other browser test) and it is nice to make sure this is not the case before disabling. Examples:
- `processor_entity.cc(263): Check failed: commit_only || data.response_version > metadata_.server_version()`, which is a sync system flake ([bug](https://bugs.chromium.org/p/chromium/issues/detail?id=1299874&q=component%3APlatform%3EWebAppProvider%3EIntegrationTesting&can=2)).
- Renderer crashes failing tests w/o stack traces ([bug](https://crbug.com/1329854#c31)).

### A user action was refactored and our driver is out of date.

In more rare scenarios, a user action (like how a user installs a web app, or launches from the intent picker, etc) changes in production code but the method in the `WebAppIntegrationTestDriver` is not updated to reflect the "new way". This would then cause all tests that use that action to fail. This should be caught in trybots, and if so, that action must be updated in the driver to match the new version.
