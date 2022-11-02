# About

This folder contains the backend implementation of Chrome OS launcher search.

# Overview of search infrastructure

## Important classes

### Core

- **SearchController**. This controls all the core search functions such as
  starting a search, collecting results, ranking and publishing. Implemented by
  `SearchControllerImplNew`.

  To interact with the frontend, it calls the `AppListController` and
  `AppListModelUpdater`, and is called by the `AppListClient`.
- **SearchProvider**. The base class for all search providers. Each search
  provider typically handles one type of result, such as settings, apps or
  files. Some search providers implement their search function locally, while
  others call out to further backends.
- **SearchControllerFactory**. Responsible for the creation of the search
  controller and its providers at start-up time.
- **ChromeSearchResult**. The base class for all search results. Each
  `ChromeSearchResult` contains the information associated with one result. This
  information is stored in a `SearchResultMetadata` object which is piped to the
  frontend code.

### Ranking

Ranking is the process of assigning scores to each result and category to
determine their final display order. Located inside the `ranking/` subdirectory.

- **RankerManager**. This owns the ranking stack and determines the order of
  ranking steps.
- **Ranker**. The base class for all rankers. Rankers can be used for all kinds
  of post-processing steps, including but not limited to ranking.

### Metrics

- **AppListNotifierImpl**. Located in the parent directory
  `chrome/browser/ui/app_list/`. Contains a state machine that converts raw UI
  events into information such as impressions and launches.
- **SearchMetricsManager**. Observes the `AppListNotifier` and logs metrics
  accordingly.

## Life of a search query

1. The user types a query into the launcher search box. This filters through UI
   code until it eventually reaches `SearchController::StartSearch(query)`.
2. The `SearchController` forwards this query to its various search providers.
3. Search providers return their results **asynchronously**.
4. The `SearchController` collects these results and performs ranking on the
   results and their categories.
5. Results are published to the UI.

Steps #3-5 may be repeated several times due to the asynchronous nature of #3.
The `BurnInController' contains timing logic to reduce the UI effect of results
popping in.

Training may be performed:

6. The user clicks on a result.
7. The 'SearchController' forwards this information to its various search
   providers and rankers, which can use this information to inform future
   searches and ranking.
