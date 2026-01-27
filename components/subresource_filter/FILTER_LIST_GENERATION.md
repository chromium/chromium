# Generating a Smaller Filter List
Filter lists can be quite large to store in memory, which is problematic on
memory constrained devices. The following steps demonstrate how to generate a
smaller filter list by filtering out the least-frequently-used rules on the top
N websites (according to Alexa rankings).

## 1. Gather the URL requests from the landing pages measured by HttpArchive
This data is made available by the [HttpArchive](https://httparchive.org/)
project and is queryable via [BigQuery](https://bigquery.cloud.google.com/). A
short introduction to querying HttpArchive data is available
[here](https://har.fyi/guides/getting-started/). Because the output of our query
is typically quite large, it's necessary to have a Google Compute Engine account
with a storage bucket created to write the resulting table to.

The query to run is:
```sql
#standardSQL

SELECT
  root_page AS origin,
  url AS request_url,
  type AS request_type,
  rank as site_rank,
FROM
  `httparchive.latest.requests`
WHERE
  -- httparchive's database includes data from sub-pages. Our filter list has
  -- historically only dealt with requests that originate from root pages, so we
  -- need to filter the sub-page requests out.
  is_root_page = true AND
  rank < 5000000 AND
  -- Use a partition elimination filter to prevent querying the entire dataset.
  -- 61 days to account for July 1-August 31 range.
  date BETWEEN DATE_SUB(CURRENT_DATE(), INTERVAL 61 DAY) AND
  CURRENT_DATE()
```

Since the output is too large (>32GB) to display on the page, the results will
need to be written to a table in your Google Cloud Project. To do this, press
the 'show options' button below your query, and press the 'select table' button
to create a table to write to in your project. You'll also want to check the
'allow large results' checkbox.

Now run the query. The results should be available in the table you specified
in your project. Find the table on the BigQuery page and export it in JSON
format to a bucket that you create in your Google Cloud Storage. Since files
in buckets are restricted to 1GB, you'll have to shard the file over many
files. Select gzip compression and use `<your_bucket>/site_urls.*.json.gz` as
your destination.

Once exported, you can download the files from your bucket.

## 2. Acquire a filter list in the indexed format
Chromium's tools are designed to work with a binary indexed version of filter
lists. You can use the `subresource_indexing_tool` to convert a text based
filter list to an indexed file.

The `derive_filterlist.sh` script oversees the automated process of downloading
filterlist files, matching their rules against the requests gathered in the
previous step, finding the most popular rules, and writing them out in
an indexed format that Chromium can read.

```sh
1. bash components/subresource_filter/tools/derive_filterlist.sh <out directory path> <path to files downloaded from step 1> <path to output>
Example: bash components/subresource_filter/tools/derive_filterlist.sh ~/chromium/src/out/release ~/filterlist_data/pages/ ~/filterlist_data/out/
```

# Generating and using a mock ruleset for development/testing

It can be useful for development and testing to create a custom ruleset to activate the filter on your testing page:

1. Grab easylist: https://easylist.to/easylist/easylist.txt or create your own, see https://adblockplus.org/filter-cheatsheet for the format. Here's an example `mock_easylist.txt`:
    ```
    ||mockad.glitch.me^
    ```
    Will filter a child frame coming from `http(s)://mockad.glitch.me`.
2. Build tools needed to build the ruleset: `autoninja -C out/Release subresource_filter_tools`
3. Run `./out/Release/ruleset_converter --input_format=filter-list --output_format=unindexed-ruleset --input_files=mock_easylist.txt --output_file=mock_easylist_unindexed`
4. In `chrome://components` ensure "Subresource Filter Rules" has a non-0 version number or click "Check For Update". This ensures the path used in the following steps is created.
5. In your Chrome user-data-dir, go to the `Subresource Filter/Unindexed` directory. Locate the latest version directory and increment the number: e.g. `mv 9.34.0/ 9.34.1/` (note: long-term, Chrome may replace this with a real list again when a new version is found).
6. Update the `version` property in `manifext.json` to match the incremented version number
7. Overwrite `Filtering Rules` with the unindexed ruleset generated in step 3: `cp $CHROME_DIR/mock_easylist_indexed ./Filtering\ Rules`
8. Remove `manifest.fingerprint` and `\_metadata`, leaving just `Filtering Rules`, `LICENSE.txt`, and `manifest.json`: `rm -rf manifest.fingerprint _metadata`
9. Open Chrome. Confirm you're on the incremented version in chrome://components. A matching version directory should be created in `Subresource Filter/Indexed`, e.g. `Subresource Filter/Indexed Rules/35/9.34.1`

The ruleset is now loaded in Chrome but filtering will only occur on a pages
that have the ad blocker activated. Activation can be simulated using the "Force ad
blocking on this site" option in [DevTools Settings](https://www.chromium.org/testing-chrome-ad-filtering/).
