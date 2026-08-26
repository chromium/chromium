# Page Content Extraction Service (PCES)

The Page Content Extraction Service (PCES) is a browser-process coordination
layer that orchestrates the extraction, caching, and distribution of structured
page content, primarily `optimization_guide::proto::AnnotatedPageContent` (APC)
for HTML pages, but plain text for PDFs.

## High-level architecture

PCES provides a unified wrapper over lower-level extraction APIs.

*   **`PageContentExtractionService`**: A Profile-keyed service that manages
    caches, schedules cleanups, and notifies observers.
*   **`AnnotatedPageContentRequest`**: A `WebContentsObserver` /
    `WebContentsUserData` helper that implements a lifecycle state machine per
    tab. It tracks load events, First Contentful Paints (FCP), visibility
    changes, and page stability.
*   **`PageSettledMonitor`**: Determines when a page has "settled" (finished
    loading and stable) before initiating extraction.
*   **`PageContextFetcher`**: Orchestrates parallel fetches of screenshots,
    inner text, and raw Mojo `AIPageContentResult`.

## Choosing between PCES and `GetAIPageContent()`

When page content is needed, PCES is generally the preferred option over calling
lower-level Mojo APIs directly, unless specific request-level customization is
required.

1.  **Shared caching**: PCES maintains an in-memory and on-disk cache for HTML
    pages. Bypassing PCES risks triggering duplicate extractions, which could
    lead to unnecessary performance costs if multiple features query the same
    page.
2.  **Page stability detection**: PCES automatically defers extractions until
    the page has finished loading and page stability is established (using
    `PageSettledMonitor`), ensuring you capture a complete, high-quality
    representation of the page.
3.  **Tab lifecycle integration**: Automatically cleans up cached data when
    tabs are closed or discarded, and, if 'on hidden' triggering is enabled,
    schedules extractions when visible tabs transition to the background.

## API comparison: PCES vs. `GetAIPageContent()`

| Feature / aspect | `PageContentExtractionService` (recommended) | `optimization_guide::GetAIPageContent()` (Mojo API) |
| :--- | :--- | :--- |
| **Caching** | Yes (In-Memory + On-Disk SQLite) | No (Always fetches from Renderer) |
| **Orchestration** | Automated (waits for page to load & settle) | Manual (must be triggered by caller) |
| **Duplicate extractions** | Avoided (cached content can be reused across callers) | Possible (each call triggers a separate extraction from the renderer) |
| **Custom options** | Static (defined globally via feature flags) | Dynamic (caller can customize per request) |

## Usage examples

### 1. Registering an observer

Use this pattern to automatically listen for page content extractions as they
happen (e.g. for background processing or history indexing).

> **Note:** Changes to automatic observer extractions are being considered
> given the potential overhead of automatic extractions across all loaded
> pages.

```cpp
#include "components/page_content_annotations/content/page_content_extraction_service.h"

class MyFeatureObserver
    : public page_content_annotations::PageContentExtractionService::Observer {
 public:
  MyFeatureObserver(
      page_content_annotations::PageContentExtractionService* service) {
    observation_.Observe(service);
  }

  // PageContentExtractionService::Observer:
  void OnPageContentExtracted(
      content::Page& page,
      page_content_annotations::PageContent page_content) override {
    if (auto apc_ptr =
            page_content_annotations::GetAnnotatedPageContentPtrFromPageContent(
                page_content)) {
      const optimization_guide::proto::AnnotatedPageContent& proto =
          apc_ptr->data;
      // Process APC (AnnotatedPageContent proto)...
    }
  }

 private:
  base::ScopedObservation<
      page_content_annotations::PageContentExtractionService,
      page_content_annotations::PageContentExtractionService::Observer>
      observation_{this};
};
```

### 2. Requesting content asynchronously (on-demand)

Use this to fetch the content for a page. If the content is cached, the callback
resolves immediately. If not, PCES will schedule and trigger a new extraction.

```cpp
page_content_extraction_service
    ->GetExtractedPageContentAndEligibilityForPageAsync(
        page,
        base::BindOnce(
            [](std::optional<
                page_content_annotations::ExtractedPageContentResult> result) {
              if (result) {
                // Use result->page_content (AnnotatedPageContent proto)...
              }
            }),
        /*trigger_if_not_cached=*/true);
```

### 3. Forcing a refresh (on-demand)

If you know you need a fresh extraction (e.g. the page content has changed
dynamically since initial load, or your feature requires recent data rather than
a cached result), you can force a refresh.

```cpp
page_content_extraction_service
    ->RefreshExtractedPageContentAndEligibilityForPage(
        page,
        base::BindOnce(
            [](std::optional<
                page_content_annotations::ExtractedPageContentResult> result) {
              if (result) {
                // Process refreshed content...
              }
            }));
```
