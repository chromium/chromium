# History Component

Component that manages visited URLs history.

## History services

There are 3 different history services that fulfill different requirements:

* [HistoryService](/components/history/core/browser/history_service.h): manages
and allows access to local-only history.
* [WebHistoryService](/components/history/core/browser/web_history_service.h):
handles queries to history servers providing overall history access (local and
remote), potentially including from other devices synced to the same account.
* [BrowsingHistoryService](/components/history/core/browser/browsing_history_service.h):
is a layer on top of the two services above and
[SyncService](/components/sync/service/sync_service.h) that works transparently
for both sync'ing and non-sync'ing users.
