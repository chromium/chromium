This directory is concerned with the management of Lacros's startup parameters.

Startup parameters are passed by Ash's `BrowserManager` to Lacros via
`memfd`s. The fd numbers are passed to Lacros via command line arguments
defined in `startup_switches`.


### `BrowserInitParams` in production vs. testing

In production, Lacros is invoked by Ash with the
`--crosapi-mojo-platform-channel-handle` command line switch pointing to a file
descriptor. This causes Lacros to populate `BrowserInitParams` with Ash-provided
data read from the file. This happens whenever the `BrowserInitParams` are
queried for the first time.

In testing where Lacros is not invoked by Ash (e.g. Lacros unit and browser
tests) there are two scenarios:

1) Lacros is invoked without crosapi command line switch. Then
   `BrowserInitParams` is created empty. This is the case for example for
   `unit_tests` and `browser_tests`. Crosapi is thereby effectively disabled.

2) Lacros is invoked with crosapi command line switch. Then `BrowserInitParams`
   are read from Ash just as in production *unless* there's a call to
   `DisableCrosapiForTesting`, in which case the behavior follows that in (1).
   (An assertion ensures that `DisableCrosapiForTesting` is called before the
   first query of `BrowserInitParams`, if at all).

   Detail: For technical reasons, the command line switch here is not
   `--crosapi-mojo-platform-channel-handle` but
   `--lacros-mojo-socket-for-testing`. The test runner passes a socket through
   which `BrowserTestBase` then requests the
   `--crosapi-mojo-platform-channel-handle` file descriptor from Ash.

Only `lacros_chrome_browsertests` and Lacros `interactive_ui_tests` follow (2),
with only very few uses of `DisableCrosapiForTesting`.
