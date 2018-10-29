Support files for OriginPolicyBrowserTest.

An origin policy needs to be announced by the server in a header, and so this
directory contains several files with ".mock-http-headers" companion files
announcing different origin policies. The test looks for the page title, and so
the headers file and the title in the .html file are the main "payload" for
the test. Additionally, the .well-known/origin-policy/ directory contains
example policies.

The main test suite for Origin Policy is wpt/origin-policy. These tests
concentrate on aspects that can't easily be tested in the cross-browser WPT
suite, mainly error handling.
