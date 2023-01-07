# Language Packs

Language Packs are a ChromeOS layer that allows language-specific assets to be
bundled together and downloaded at run time.

This directory contains the logic that powers Language Packs.

The main logic runs in Ash process (ash-chrome) and it allows clients to query
and retrieve language packs for a specific language.

The pack is verified, extracted and mounted to the user partition.

A Mojo API is also provided, in order for clients outside the browser
process to communicate via IPC.

For the documentation see go/g3d-languagepacks.

## Sample Usage

In the Browser Process you can install a Language Pack via:

```
LanguagePackManager::GetInstance()->InstallPack(
    "Feature ID", "en-US", std::move(callback));
```

The callback will be called when the operation ends, either with success or
failure. In the case of success, the path to the extracted files is returned in
the callback.
