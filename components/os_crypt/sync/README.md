***OS Crypt (Sync)***

This directory contains an `OSCrypt` implementation that supports cryptographic
primitives that allow binding data to the OS user.

[os_crypt.h](os_crypt.h) contains the main interface.

The interface supports both instance based and a singleton interface, most
callers will use the singleton interface via convenience functions that handle
obtaining the singleton and calling directly into it. Advanced usage can
directly create an `OSCryptImpl` if needed, or access the singleton via
`GetInstance`.

Initialization is done per-process and can is usually done by calling the
platform-specific initialization function, which should take place before any
calls to encrypt or decrypt data occur.

*   Linux - `SetConfig`
*   Windows - `Init`

Alternatively, `OSCrypt` can be initialized with a key directly by using
`SetRawEncryptionKey` (or `InitWithExistingKey` - Windows only). This can also
be used to initialize OSCrypt in a non-browser process using a key supplied by
the browser.

The main functions are `EncryptString` and `DecryptString`. These can be called
on any thread and will return a user-bound encrypted string. It is guaranteed
that a string encrypted with `EncryptString` will be able to successfully
decrypt if `DecryptString` is called in the same user context. The exact
definition of user context is OS defined.
