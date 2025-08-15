# Search Engines Component

## What is the Search Engines Component?

This component provides the core logic for managing search engines in Chromium. It includes the data structures and services for storing, retrieving, and managing the set of available search engines. It is also responsible for determining and setting the default search engine.

The component handles a variety of search engine-related tasks, including:
*   **Persistence**: Storing and retrieving search engine data from the web database.
*   **Sync**: Syncing search engines across devices using `syncer::SyncableService`.
*   **Prepopulation**: Providing a default set of search engines based on the user's country.
*   **Dynamic Fetching**: Discovering and adding new search engines from websites via OpenSearch Description Documents (OSDD).
*   **Default Search Management**: Determining the default search engine based on a hierarchy of sources (policy, extensions, user choice, and fallback).

## How to Use the Search Engines Component

The primary entry point for interacting with this component is the `TemplateURLService`. All modifications to the set of search engines must be done through this service to ensure that changes are correctly persisted and synchronized.

### Key Classes

*   **`TemplateURLService`**: A `KeyedService` that manages the lifecycle of `TemplateURL` objects. Use this service to add, remove, update, and retrieve search engines.
*   **`TemplateURL`**: Represents a single search engine, containing its name, keyword, and various URL templates (for search, suggestions, etc.).
*   **`TemplateURLData`**: A plain data struct that holds all the information about a search engine. It is used to create `TemplateURL` objects and for persistence and synchronization.
*   **`DefaultSearchManager`**: Determines the default search engine based on a hierarchy of sources: policy, extensions, user settings, and prepopulated data.
*   **`KeywordWebDataService`**: A specialization of `WebDataServiceBase` that manages the `keywords` table in the web database. It is responsible for persisting search engine data.
*   **`TemplateURLFetcher`**: A `KeyedService` that asynchronously downloads OpenSearch Description Documents (OSDD) from websites to discover and add new search engines.
*   **`TemplateURLParser`**: Parses OpenSearch Description Documents and creates `TemplateURL` objects from them.

## Lay of the Land

The `components/search_engines` directory is structured as follows:

*   **`template_url_service.h` / `.cc`**: The main service for managing search engines.
*   **`template_url.h` / `.cc`**: The data model for a single search engine.
*   **`default_search_manager.h` / `.cc`**: Logic for managing the default search provider.
*   **`keyword_web_data_service.h` / `.cc`**: Handles persistence of search engine data to the `WebDatabase` via the `KeywordTable`.
*   **`template_url_prepopulate_data.h` / `.cc`**: Provides the initial, country-specific set of search engines.
*   **`template_url_fetcher.h` / `.cc`**: Asynchronously downloads OpenSearch Description Documents (OSDD) to add new search engines discovered on websites.
*   **`android/`**: Contains Android-specific logic for search engines.
*   **`enterprise/`**: Contains logic for managing search engines in an enterprise environment, including support for policy-defined search providers.
*   **`search_engine_choice/`**: Contains logic related to the search engine choice screen, which is shown to users in certain regions (e.g., the EEA).

## Contacts

For questions about this component, please contact the owners listed in the `OWNERS` file.

*   **Search Engine Choice:** dgn@chromium.org
*   **Sync:** ankushkush@google.com
*   **General:** file://components/omnibox/OWNERS
