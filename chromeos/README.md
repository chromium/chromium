# Chrome OS

This directory contains low-level support for Chrome running on Chrome OS.

The [Lacros project](go/lacros) is in the process of extracting the
browser-functionality into a separate binary. This introduces the following
terminology and rules:
  * ash-chrome: The new name of the legacy "chrome" binary. It contains system
    UI and the current/legacy web browser. Code that is only used by ash-chrome
    should eventually be moved to //chromeos/ash, have an _ash suffix in
    the filename, or have a (grand-)parent directory named /ash/.
  * lacros-chrome: The name of the new, standalone web-browser binary. Code that
    is only used by lacros-chrome should have a _lacros suffix in the filename,
    or have a (grand-)parent directory named /lacros/.
  * crosapi: The term "crosapi" is short for ChromeOS API. Ash-chrome
    implements the API, and lacros-chrome is the only consumer.
  * chromeos: The term "chromeos" refers to code that is shared by binaries
    targeting the chromeos platform or using the chromeos toolchain. Code that
    is shared by ash-chrome and lacros-chrome should have a _chromeos suffix in
    the filename, or have a (grand-)parent directory named /chromeos/.
  * Exception: The exception to the rule is //chrome/browser/chromeos. Following
    existing conventions in //chrome, the directory *should* refer to
    lacros-chrome. However, this would involve a massive and otherwise
    unnecessary refactor. //chrome/browser/chromeos will continue to contain
    code that is only used by ash-chrome. //chrome/browser/lacros will contain
    code used only by lacros-chrome.
See [this document](go/lacros-code-layout) for more details.

Many subdirectories contain Chrome-style C++ wrappers around operating system
components.

For example, //chromeos/dbus contains wrappers around the D-Bus interfaces to
system daemons like the network configuration manager (shill). Most other
directories contain low-level utility code.

There are two exceptions:

- //chromeos/services contains mojo services that were not considered
  sufficiently general to live in top-level //services and that, at the same
  time, are shared between ash-chrome and lacros-chrome. In case of an
  ash-chrome only mojo service, please use //chromeos/ash/services instead.

- //chromeos/components contains C++ components that were not considered
  sufficiently general to live in top-level //components.

Note, //chromeos does not contain any user-facing UI code, and hence it has
"-ui" in its DEPS. The contents of //chromeos should also not depend on
//chrome.
