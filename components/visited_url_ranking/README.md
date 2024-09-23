# Visited URL Ranking Component

Component that facilitates fetching and ranking of aggregated user URL visit
data managed by different sources (e.g. Tab Model, Session, History).

## Visited URL Ranking services

There is a common interface to fulfill data fetching requirements:

* [URLVisitDataFetcher](/components/visited_url_ranking/public/url_visit_fetcher.h)
Common interface that facilitates fetching and aggregation of URL visit data
across different sources.

There are different services that fulfill different requirements:
* [SessionURLVisitDataFetcher](/components/visited_url_ranking/internal/session_url_visit_data_fetcher.h)
Service that facilitates fetching and aggregating data from the `session`
source.
* [HistoryURLVisitDataFetcher](/components/visited_url_ranking/internal/history_url_visit_data_fetcher.h)
Service that facilitates fetching and aggregating data from the `history`
source.
