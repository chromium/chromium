This directory is concerned with the management of Lacros's startup parameters.

Startup parameters are passed by Ash's `BrowserManager` to Lacros via
`memfd`s. The fd numbers are passed to Lacros via command line arguments
defined in `startup_switches`.

Startup parameters belong to two classes: `BrowserInitParams` and
`BrowserPostLoginParams`. That's because Lacros supports being
pre-launched at login screen, when user-specific parameters are
not known yet. Accessing startup parameters via `BrowserPostLoginParams`
before login blocks, waiting for login.

`BrowserInitParams` and `BrowserPostLoginParams` are not to be used
directly. Instead, Lacros code should use `BrowserParamsProxy` which
dispatches to either `BrowserInitParams` and `BrowserPostLoginParams`
based on whether Lacros was prelaunched at login screen or not.
