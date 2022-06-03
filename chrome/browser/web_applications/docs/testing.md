# [Web Apps](../README.md) - Testing


Testing in WebAppProvider falls into 3 different categories.
1. Unit tests (`*_unittest.cc` files), which are the most efficient.
1. Browser tests (`*_browsertest.cc` files), which run the whole Chrome browser for each test. This makes them less efficient, but possible to test interactions between different parts of Chrome.
   * Note: These are currently not being run on Mac CQ trybots (see https://crbug.com/1042757), but they are run on the waterfall.
1. [Integration tests](../../ui/views/web_apps/README.md), which are a special kind of browser-test-based framework to test our critical user journeys.

When creating features in this system, it will probably involve creating a mixture of all 3 of these test types.

Please read [Testing In Chromium](../../../../docs/testing/testing_in_chromium.md) for general guidance on writing tests in chromium.

## Future Improvements

* Allow easy population of a [`FakeWebAppProvider`](../test/fake_web_app_provider.h) from a [`FakeWebAppRegistryController`](../fake_web_app_registry_controller.h).

## Terminology

### `Fake*` or `Test*` classes

A class that starts with `Fake` or `Test` is meant to completely replace a component of the system. They should be inheriting from a base class (often pure virtual) and then implement a version of that component that will seem to be working correctly to other system components, but not actually do anything.

An example is [fake_os_integration_manager.h](../test/fake_os_integration_manager.h), which pretends to successfully do install, update, and uninstall operations, but actually just does nothing.

### `Mock*` classes

A class that start with `Mock` is a [gmock](https://github.com/google/googletest/tree/HEAD/googlemock) version of the class. This allows the user to have complete control of exactly what that class does, verify it is called exactly as expected, etc. These tend to be much more powerful to use than a `Fake`, as you can easily specify every possible case you might want to check, like which arguments are called and the exact calling order of multiple functions, even across multiple mocks. The downside is that they require creating a mock class & learning how to use gmock.

An example is [MockOsIntegrationManager](../os_integration_manager_unittest.cc) inside of the unittest file.

## Unit tests
Unit tests have the following benefits
* are very efficient,
* run on all relevant CQ trybots (while https://crbug.com/1042757 is not fixed), and
* will always be supported by the [code coverage](../../../../docs/testing/code_coverage.md) framework.

The downside is that it can be difficult to test interactions between different parts of our system in Chrome (which can range from blink with the [`ManifestFetcher`](https://source.chromium.org/search?q=ManifestFetcher) to the install dialog in [`PWAInstallView`](https://source.chromium.org/search?q=PWAInstallView)).

Unit tests usually rely on "faking" or "mocking" out dependencies to allow one specific class to be tested, without requiring the entire WebAppProvider (and thus Profile, Sync Service, etc) to be fully running. This is accomplished by having major components:
1. declare all public methods as `virtual` so that a `Fake`, `Test`, or `Mock` version of the class can be used instead, and
1. accept all dependencies in their constructor or `SetSubsystems` method.

This allows a unittest to create a part of the WebAppProvider system that uses all mocked or faked dependencies, allowing easy testing.


### Tool: `FakeWebAppRegistryController`

The [`FakeWebAppRegistryController`](../test/fake_web_app_registry_controller.h) is basically a fake version of the WebAppProvider system, without using the [`WebAppProvider`](../web_app_provider.h) root class. This works well as long as none of the parts of the system are using [`WebAppProvider::Get`](https://source.chromium.org/search?q=WebAppProvider::Get), as this uses they `KeyedService` functionality on the `Profile` object, and the `FakeWebAppRegistryController` doesn't register itself with any of that.

`FakeWebAppRegistryController::database_factory()` is special: it allows you to programmatically create some LevelDB state (an offline registry snapshot) before any subsystem starts. This is useful to customize inputs and preconditions in unit tests. To do this, or for examples, see [`TestWebAppDatabaseFactory::WriteProtos`](https://source.chromium.org/search?q=TestWebAppDatabaseFactory::WriteProtos) and [`TestWebAppDatabaseFactory::WriteRegistry`](https://source.chromium.org/search?q=TestWebAppDatabaseFactory::WriteRegistry).

### Common issues & solutions

#### Dependency not passed in normally
Sometimes classes have not used the dependency pattern, or rely on pulling things off of the `Profile` keyed services. This can be solved by
1. Refactoring that class a little to have the dependency passed in the constructor / `SetSubsystems` method.
1. There should be a way to register a keyed service factory on a given `Profile` to return what you want.
1. If all else fails, use a browser test

## Browser tests
Browser tests are much more expensive to run, as they basically run a fully functional browser with it's own profile directory. These tests are usually only created to test functionality that requires multiple parts of the system to be running or dependencies like the Sync service to be fully running and functional.

Browsertest are great as integration tests, as they are almost completely running the full Chrome environment, with a real profile on disk. It is good practice to have browsertests be as true-to-user-action as possible, to make sure that as much of our stack is exercised.

A good example set of browser tests is in [`web_app_browsertest.cc`](../../ui/web_applications/web_app_browsertest.cc).

### Tool: `FakeWebAppProvider`

The [`FakeWebAppProvider`](../test/fake_web_app_provider.h) is a nifty way to mock out pieces of the WebAppProvider system for a browser test. To use it, you put a [`FakeWebAppProviderCreator`](../test/fake_web_app_provider.h) in your test class, and give it a callback to create a `WebAppProvider` given a `Profile`. This allows you to create a [`FakeWebAppProvider`](../test/fake_web_app_provider.h) instead of the regular `WebAppProvider`, swapping out any part of the system.

This means that all of the users of [`WebAppProvider::Get`](https://source.chromium.org/search?q=WebAppProvider::Get), [`WebAppProvider::GetForWebContents`](https://source.chromium.org/search?q=WebAppProvider::Get) (etc) will be talking to the `FakeWebAppProvider` that the test created. This is perfect for a browsertest, as it runs the full browser.

The other difference between this and the [`FakeWebAppRegistryController`](#tool-testwebappregistrycontroller) above is that this, without any changes (and as long as the user calls `Start()`), will run the normal production `WebAppProvider` system. This means changes are written to disk, the OS integrations are triggered, etc.

## Integration tests
Due to the complexity of the WebApp feature space, a special testing framework was created to help list, minimize, and test all critical user journeys. See the [README.md here](../../ui/views/web_apps/README.md) about how to write these.
