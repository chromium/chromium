# Query Tiles Service

## Introduction
Query tiles is a new feature to bring down the barriers for NIU users and boost
their confidence when using Chrome in Android.  It encourages users to explore
the internet by simply clicking on a few image tiles, and building a search
query to find the information they want.

With this feature enabled, users will see a list of image tiles with the
corresponding query strings shown on NTP(new tab page) and Omnibox suggestions.
By clicking on those tiles, users will be directed to the search result page
provided.

The client side will schedule fetch tasks from Google server periodically to
update the latest data and images based on the user's locale.

## Code structure

[components/query_tiles](.)
Public interfaces and data structure.

[components/query_tiles/internal](./internal)
internal implementations.

[components/query_tiles/proto](./proto)
Protobuf structure.

[components/query_tiles/android](./android)
UI related code.

[chrome/browser/query_tiles](../../chrome/browser/query_tiles)
include service factory and background task client code.

[components/browser_ui/widget/image_tiles](../browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/image_tiles/)
Generic widget representing image tiles carousel.

`TileService` - Public interface for query tile service.

`TileServiceScheduer` - Handles scheduling tasks to fetch tiles from the server.

`TileManager` - Store and dispense query tiles.

`ImageTileCoordinator` - UI widget representing the carousel.

`TileProvider` - Handles backend interaction from the Java layer

`QueryTileSection` - Query tiles widget on NTP.

`QueryTileProvider` - Provider for omnibox query tile suggestions.


## Test and debug

### Feature flags
In chrome://flags,

* Disable `Start surface`

* Enable `Query tiles`

* Enable `Query tiles in omnibox`

* Set `Country code for getting tiles` to `IN` or `US`

* If you’d like to see the UI immediately, enable `Query tile instant fetch`,
restart Chrome twice and wait for 10 seconds.

### WebUI
Use `chrome://internals/query-tiles` to manually tune the flow(start/reset) and
show the internal status and data.
