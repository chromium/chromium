# ChromeDriver

This file contains high-level info about how ChromeDriver works and how to
contribute. If you are looking for information on how to use ChromeDriver,
please see the [ChromeDriver user site](https://chromedriver.chromium.org/).

ChromeDriver is an implementation of the
[WebDriver standard](https://w3c.github.io/webdriver/),
which allows users to automate testing of their website across browsers.

## Getting Started

ChromeDriver source code is located in the Chromium source repository,
and shares the same build tools as Chromium.
To build ChromeDriver, please first follow the instructions to
[download and build Chromium](https://www.chromium.org/developers/how-tos/get-the-code).

Once you have set up the build environment,
build ChromeDriver by building the `chromedriver` target, e.g.,

```
autoninja -C out/Default chromedriver
```

This will create an executable binary in the build folder named
`chromedriver[.exe]`.

Once built, ChromeDriver can be used with various third-party libraries that
support WebDriver protocol, including language bindings provided by Selenium.

Note that if your build target OS is Android (i.e., you have
`target_os = "android"` in your `args.gn` file), you will need to use the
following command to build ChromeDriver targeting the host system:

```
autoninja -C out/Default clang_x64/chromedriver
```

### Verifying the Build

For testing purposes, ChromeDriver can be used interactively with python.
The following is an example on Linux. It assumes that you downloaded Chromium
repository at ~/chromium/src, and you used out/Default as the build location.
You may need to adjust the paths if you used different locations.
The following code uses our own testing API, not the commonly used Python
binding provided by Selenium.

```python
$ cd ~/chromium/src/chrome/test/chromedriver
$ export PYTHONPATH=$PWD:$PWD/server:$PWD/client
$ python3
>>> import server
>>> import chromedriver
>>> cd_server = server.Server('../../../out/Default/chromedriver')
>>> driver = chromedriver.ChromeDriver(cd_server.GetUrl(), cd_server.GetPid())
>>> driver.Load('http://www.google.com')
>>> driver.Quit()
>>> cd_server.Kill()
```

By default, ChromeDriver will look in its own directory for Chrome to use.
If Chrome is not found there, it will use the system installed Chrome.

To use ChromeDriver with Chrome on Android pass the Android package name in the
chromeOptions.androidPackage capability when creating the driver. You also need
to have Android SDK's Android Debug Bridge (adb) server running. For
more detailed instructions see the [user site](https://chromedriver.chromium.org/getting-started/getting-started---android).

## Architecture

ChromeDriver is shipped separately from Chrome. It controls Chrome out of
process through [DevTools](https://chromedevtools.github.io/devtools-protocol/).
ChromeDriver is a standalone server which
communicates with the WebDriver client via the WebDriver wire protocol, which
is essentially synchronous JSON commands over HTTP. WebDriver clients are
available in many languages, and many are available from the open source
[Selenium WebDriver project](https://www.selenium.dev/).
ChromeDriver uses the webserver from
[net/server](https://source.chromium.org/chromium/chromium/src/+/main:net/server/).

Additional information is available on the following pages:
* [Threading](docs/threading.md): ChromeDriver threading model.
* [Chrome Connection](docs/chrome_connection.md):
  How ChromeDriver connects to Chrome and controls it.
* [DevTools Event Listener](docs/event_listener.md):
  How ChromeDriver responds to events from Chrome DevTools.
* [Run JavaScript Code](docs/run_javascript.md):
  How ChromeDriver sends JavaScript code to Chrome for execution.

## Code structure (relative to this file)

* [(this directory)](.):
Implements chromedriver commands.

* [chrome/](chrome/):
A basic interface for controlling Chrome. Should not depend on or reference
WebDriver-related code or concepts.

* [js/](js/):
Javascript helper scripts.

* [net/](net/):
Code to deal with network communication, such as connection to DevTools.

* [client/](client/):
Code for a python client.

* [server/](server/):
Code for the chromedriver server.
A python wrapper to the chromedriver server.

* [test/](test/):
Integration tests.

## Testing

There are several test suites for verifying ChromeDriver's correctness.
For details, see the [testing page](docs/testing.md).

## Releasing

As of M115, ChromeDriver is fully integrated into the Chrome release process. Across all release channels (Stable, Beta, Dev, Canary), every Chrome release that gets pushed to users automatically gets a correspondingly-versioned ChromeDriver release, [available for download via Chrome for Testing infrastructure](https://googlechromelabs.github.io/chrome-for-testing/).

## Contributing

Find an open issue and submit a patch for review by an individual listed in
the `OWNERS` file in this directory. Issues are tracked in
[ChromeDriver issue tracker](https://crbug.com/chromedriver).
