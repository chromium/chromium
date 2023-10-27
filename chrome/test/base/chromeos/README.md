This is for Lacros testing on Linux.

This directory contains code that is compiled into Ash-chrome.
The tests themselves are compiled into binaries based on Lacros,
which communicate to this copy of Ash-chrome over crosapi.

When you need to write a Lacros gtest(unit test, browser test, etc), and
you need to change Ash's behavior, you can change here.
As an example, ml service client library requires the ml service daemon,
which is not present in the unit test or browser test environment. So they
need a fake ml service in ash. With that, they need to install the fake
component in
//chrome/test/base/chromeos/test_ash_chrome_browser_main_extra_parts.cc.

For Ash browser test, please see DemoAshRequiresLacrosTest. It will first
start ash as wayland server, with no ash browser window opened. Then start
Lacros browser. At the end, start Ash browser window.
