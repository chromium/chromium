# Image Fetcher

Image Fetcher downloads image data from a URL and can decode it into a
`gfx::Image`. It provides network-only and cached fetchers for Chromium
features that need to retrieve images.

## Using Image Fetcher

In C++, [`ImageFetcher`](core/image_fetcher.h) can return a decoded image with
`FetchImage()`, encoded data without decoding with `FetchImageData()`, or both
with `FetchImageAndData()`. Supply
[`ImageFetcherParams`](core/image_fetcher.h) with a network traffic annotation
and a UMA client name. The callbacks return an empty image or string on
failure; request metadata includes the HTTP response code or network error
when available.

[`ImageFetcherService`](core/image_fetcher_service.h) provides network-only and
disk-cached fetchers through `ImageFetcherConfig`. The disk-cached fetcher
checks the cache before fetching from the network.

On Android, obtain an [`ImageFetcher`](android/java/src/org/chromium/components/image_fetcher/ImageFetcher.java)
through [`ImageFetcherFactory`](android/java/src/org/chromium/components/image_fetcher/ImageFetcherFactory.java).
The factory supports network-only, disk-cached, and in-memory cached fetchers.
The in-memory bitmap cache can be used with or without the disk cache.
Call `destroy()` when the Java fetcher is no longer needed.

## Code layout

*   [`core/`](core/) contains the C++ fetcher interfaces and implementations,
    image decoding, request metadata, and metrics.
*   [`core/cache/`](core/cache/) contains the disk image cache and its data and
    metadata stores.
*   [`android/`](android/) contains the Java API, JNI bridge, and Java tests.
*   [`ios/`](ios/) contains iOS image decoding and data fetch wrappers.
*   [`image_fetcher_bridge.cc`](image_fetcher_bridge.cc) implements the native
    side of the Android bridge.
*   [`image_fetcher_service_provider.cc`](image_fetcher_service_provider.cc)
    provides Android embedder callbacks for the service and cache path.

The C++ unit tests are alongside the code they cover in `core/` and
`core/cache/`. Android JUnit tests are in `android/junit/`, and iOS unit tests
are in `ios/`.

For component ownership, see [`OWNERS`](OWNERS).
