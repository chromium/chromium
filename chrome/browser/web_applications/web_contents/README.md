# [WebAppProvider](../README.md) > Web Contents Subsystem

## Overview

This directory contains classes that are responsible for interacting with
`content::WebContents` to retrieve information about a web app. This allows the
WebAppProvider system to wrap functionality it uses from a
`content::WebContents` in a fake-able dependency, enabling easy unit testing of
the system without having to interact with a real `content::WebContents`, which
isn't well supported in unit tests.

This also means that the `WebContentsManager`'s implementation needs to also be
thoroughly tested to make sure it performs as expected with a real
`content::WebContents`.

## Usage

The `WebContentsManager` is owned by the `WebAppProvider`. In production code,
it should not be accessed directly. Instead, it is accessed through a lock that
is acquired by a `WebAppCommand` (or via a callback scheduled via the `WebAppCommandScheduler`). The lock is then used to access the
`WebContentsManager`.

See the [`locks`](../locks/README.md) and [`commands`](../commands/README.md)
`README.md` files for more information.

## Testing (for users of the system)

For testing classes that depend on this system, `FakeWebContentsManager` can be
used to fake out the `WebContents` dependency.

## Architecture

The `WebContentsManager` is the main class in this directory. It is a factory
that is responsible for creating the following classes:

-   **`WebAppDataRetriever`**: Retrieves the `WebAppInstallInfo` from a
    `WebContents`, checks for installability, and can be used to retrieve all
    icons.
-   **`WebAppIconDownloader`**: Downloads the icons for a web app from a
    `WebContents`.
-   **`WebAppUrlLoader`**: Loads a URL in a `WebContents`. This class lives in
    [`//components/webapps/browser/web_contents/`](../../../components/webapps/browser/web_contents/).

### Testing (for developers of the system)

The classes in this directory are tested using unit tests that inherit from
`ChromeRenderViewHostTestHarness`. These tests use a `WebContentsTester` to
simulate a `WebContents` and test the logic of the classes in isolation.

See `web_app_data_retriever_unittest.cc` and
`web_app_icon_downloader_unittest.cc` for examples.

## Relevant Context

*   **Files:**
    *   `//chrome/browser/web_applications/web_contents/web_contents_manager.h`
    *   `//chrome/browser/web_applications/web_contents/web_app_data_retriever.h`
    *   `//chrome/browser/web_applications/web_contents/web_app_icon_downloader.h`
    *   `//chrome/browser/web_applications/test/fake_web_contents_manager.h`
    *   `//components/webapps/browser/web_contents/web_app_url_loader.h`
