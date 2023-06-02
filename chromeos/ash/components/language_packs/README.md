# Language Packs

Language Packs are a ChromeOS layer that allows language-specific assets to be
bundled together and downloaded at run time.

This directory contains the logic that powers Language Packs.

The main logic runs in Ash process (ash-chrome) and it allows clients to query
and retrieve language packs for a specific language.

The pack is verified, extracted and mounted to the user partition.

A Mojo API is also provided, in order for clients outside the browser
process to communicate via IPC. The Mojo API is located inside:
chromeos/ash/components/language/public/mojom/

For an in-depth guide on how to use Language Packs see go/languagepack-guide.

## Sample Usage

In the Browser Process you can install a Language Pack via the InstallPack()
function. Here's an example:

```
LanguagePackManager::GetInstance()->InstallPack(
    "Feature ID", "en-US", std::move(callback));
```

The callback will be called when the operation ends, either with success or
failure. In the case of success, the path to the extracted files is returned in
the callback.

The callback receives data as a Struct:

```
struct PackResult {
  std::string operation_error;

  enum StatusCode {
    UNKNOWN = 0,
    WRONG_ID,
    NOT_INSTALLED,
    IN_PROGRESS,
    INSTALLED
  };

  StatusCode pack_state;

  std::string path;
};
```

The final location of the extracted files is given in `path`, but only if
`pack_state` is set to `INSTALLED`.

Similarly to the installation, clients can check the state of a pack. Here's an
example

```
LanguagePackManager::GetInstance()->GetPackState(
    "Feature ID", "en-US", std::move(callback));
```

The data returned is the same struct as above.

We also provide a function to check whether the pack exists:

```
LanguagePackManager::GetInstance()->IsPackAvailable("Feature ID", "fr-FR");
```

## Oberserver pattern

Clients can register Observers in order to be notified on the changes of
a particular pack.
