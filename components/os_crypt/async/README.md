***OS Crypt Async***

This directory contains the new version of `OSCrypt` that supports asynchronous
initialization and pluggable providers. It is intended to gradually replace
`os_crypt::OSCrypt` (AKA "OSCrypt Sync") with a new version with improved
capabilities.

**Main interfaces**

`browser/` should only be included by code that lives in the browser process. An
instance of `OSCryptAsync` should be constructed and held in browser and is
responsible for minting `Encryptor` instances. \/\/chrome holds a browser-wide
instance that's accessible from `g_browser_process` using `os_crypt_async()`
method.

`GetInstance` can be called as many times as necessary to obtain instances of
`Encryptor` that should be used for encryption operations. Note that
`GetInstance` returns a `base::CallbackListSubscription` whose destruction will
cause the callback to never run. This should be stored with the same lifetime as
the callback to ensure correct function. See documentation for
`base::CallbackList` for more on this.

When calling `GetInstance` an Encryptor hint can be supplied. This can change
the Encryption behavior of the resulting Encryptor instance, see `encryptor.h`
for details and see below. Note that all `Encryptor` returned from the same
instance of `OSCryptAsync` will always be able to decrypt each other's data.

`common/` can be included by any code in any process and allows `Encryptor`
instances to perform encrypt/decrypt operations. These `EncryptString` and
`DecryptString` operations are sync and can be called on any thread, the same as
with legacy `os_crypt::OSCrypt`.

`Encryptor` instances can be passed over mojo if necessary, as mojo traits exist
to serialize and deserialize. If an `Encryptor` instance is passed to a process
then that process will be able to decrypt any data encrypted with
`OSCryptAsync`.

It is preferred to use the `base::span` `EncryptData` and `DecryptData` APIs,
however the `EncryptString` and `DecryptString` APIs are provided for ease of
compatibility with existing callers of `os_crypt::OSCrypt`. The string and span
APIs are compatible with one another.

**Integration Guide**

`OSCryptAsync` is currently integrated into the Cookie encryption within the
network service, and this code can be used as a full end-to-end example of how
to integrate `OSCryptAsync` into your existing code that is using
`os_crypt::OSCrypt`.

There are a few considerations that are important for integrators:

1.  `GetInstance()` must be called on the same sequence that it was created on,
    which, if you are using the instance managed by \/\/chrome is the UI thread.
    Therefore, plan for your `GetInstance` calls to be made on this sequence.
    Callbacks will also arrive on this sequence. Once you have an `Encryptor` it
    can be safely passed and used on another sequence, though.
2.  Care should be taken during the rollout of any integration. In particular,
    the following three phase approach is recommended, although you might want
    to shorten this depending on your risk profile. Bear in mind that
    `os_crypt::OSCrypt` and `OSCryptAsync` are likely being used to seal
    valuable data so all precautions should be made to avoid any potential data
    loss.
    1.  **Phase 1: Convert code to async**: In this first phase, the existing
        sync code that is calling into `os_crypt::OSCrypt` should be converted
        to perform an asynchronous initialization of an Encryptor instance by
        calling `GetInstance` and handling the callback. The `Encryptor`
        instance that is returned can then be used as if the code were calling
        directly to `os_crypt::OSCrypt` but since the `Encryptor` is move-only
        it will have to be held by an object on the calling sequence to make the
        calls themselves. If multiple sequences need to make encryption calls,
        that's supported, but you'll need to get an `Encryptor` for each
        sequence and explicitly pass it to those sequences. In this phase, it is
        **highly recommended** to:
        1.  Call `GetInstance` with the `kEncryptSyncCompat` option. This will
            ask the `Encryptor` to always encrypt any data in a format that is
            backwards compatible with `os_crypt::OSCrypt`. This means that if
            any issues are found when converting the code from sync to async,
            there is no risk of any permanent data loss, and any CLs can be
            safely rolled back, or features turned off. Note that Encryptors
            obtained with this flag might not always operate correctly in all
            processes as they might fallback to OSCrypt sync internally, but are
            always safe to use from browser process.
        2.  Land the async code behind a feature, although this might not always
            be possible given the restructuring required. By both using
            `kEncryptSyncCompat` and a feature flag, the code can be iterated on
            without risk to any permanent data loss for users.
        3.  Monitor baseline metrics, to verify no guardrail metrics are
            impacted. If you code needs to perform Encrypt or Decrypt operations
            very early on in startup, then it is possible that there could be
            performance regressions as `OSCryptAsync` might not yet have
            obtained a valid `Encryptor` instance.
    2.  **Phase 2: Engage new Encryption**: Once Phase 1 has landed and no
        regressions are seen, then a feature can land that removes the
        `kEncryptSyncCompat` option passed to `GetInstance` from all calls, and
        data will now start being encrypted with the keys managed by the
        installed `OSCryptAsync` key providers in a potentially non-backwards
        compatible way. In this phase, for example, data might start being
        encrypted with App-Bound encryption on supported platforms. At this
        point you will want to double check no data loss caused by encrypting
        data with the new keys, although core `OSCryptAsync` metrics themselves
        are used as guardrails against this scenario.
    3.  **Phase 3: Re-encrypt all data**: Once Phase 2 has landed and there
        appear to be no regressions from using the new key, then all data
        currently encrypted (which will include a mix of data encrypted with
        `os_crypt::OSCrypt`, and data encrypted with `OSCryptAsync` depending on
        when it was originally encrypted) should be read in from persistent
        storage, decrypted, re-encrypted, and then and written back to
        persistent storage. This ensures that all data is now encrypted with
        `OSCryptAsync` and secured by new keys.
