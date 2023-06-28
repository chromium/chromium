# Web Environment Integrity (WEI)
https://github.com/RupertBenWiser/Web-Environment-Integrity/blob/main/explainer.md

Implementations for Web Environment Integrity can vary from platform to platform.
For example, on Android we must store key-pair identifiers called "handles" and
make calls to the Play Integrity attester. On other platforms, the flow may be
much different.

When adding WEI support for a platform, code that is specific to that platform
should be placed in the appropriate platform-specific directory (e.g. /android).
Code that can be used across platforms can be placed in a /common directory.

Currently Android is the only supported platform for WEI.
