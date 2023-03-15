***OS Crypt Async***

This directory contains the new version of `OSCrypt` that supports asynchronous
initialization and pluggable providers.

**Main interfaces**

`browser/` should only be included by code that lives in the browser process. An
instance of `OSCryptAsync` should be constructed and held in browser and is
responsible for minting `Encryptor` instances.

`GetInstance` can be called as many times as necessary to obtain instances of
`Encryptor` that should be used for encryption operations. Note that
`GetInstance` returns a `base::CallbackListSubscription` whose destruction will
cause the callback to never run. This should be stored with the same lifetime as
the callback to ensure correct function. See documentation for
`base::CallbackList` for more on this.

`common/` can be included by any code in any process and allows `Encryptor`
instances to perform encrypt/decrypt operations. These `EncryptString` and
`DecryptString` operations are sync and can be called on any thread, the same as
with legacy `os_crypt::OSCrypt`.

It is preferred to use the `base::span` `EncryptData` and `DecryptData` APIs,
however the `EncryptString` and `DecryptString` APIs are provided for ease of
compatibility with existing callers of `os_crypt::OSCrypt`. The string and span
APIs are compatible with one another.
