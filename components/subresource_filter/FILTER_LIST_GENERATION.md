# Generating a Smaller Filter List
Filter lists can be quite large to store in memory, which is problematic on
memory constrained devices. The following steps demonstrate how to generate a
smaller filter list by filtering out the least-frequently-used rules on the top
N websites (according to Alexa rankings).

## 1. Gather the URL requests from the landing pages measured by HttpArchive
This data is made available by the [HttpArchive](https://httparchive.org/)
project and is queryable via [BigQuery](https://bigquery.cloud.google.com/). A
short introduction to querying HttpArchive data is available
[here](https://www.igvita.com/2013/06/20/http-archive-bigquery-web-performance-answers/).
Because the output of our query is typically quite large, it's necessary to
have a Google Compute Engine account with a storage bucket created to write
the resulting table to.

The query to run is:
```sql
#standardSQL

SELECT
  pages.url AS origin,
  requests.url AS request_url,
  requests.type AS request_type
FROM
    `httparchive.summary_requests.2018_07_15_desktop` AS requests
INNER JOIN (
  SELECT
    pageid,
    url
  FROM
    `httparchive.summary_pages.2018_07_15_desktop`) AS pages
ON
  requests.pageid = pages.pageid
UNION ALL

SELECT
  pages.url AS origin,
  requests.url AS request_url,
  requests.type AS request_type
FROM
    `httparchive.summary_requests.2018_07_15_mobile` AS requests
INNER JOIN (
  SELECT
    pageid,
    url
  FROM
    `httparchive.summary_pages.2018_07_15_mobile`) AS pages
ON
  requests.pageid = pages.pageid;
```

You'll need to replace the tables with those of the dates that you're interested in.

Since the output is too large (>32GB) to display
on the page, the results will need to be written to a table in your Google
Cloud Project. To do this, press the 'show options' button below your query, and press the
'select table' button to create a table to write to in your project. You'll
also want to check the 'allow large results' checkbox.

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

An example using [EasyList](https://easylist.to/easylist/easylist.txt) follows:

```sh
1. ninja -C out/Release/ subresource_filter_tools
2. wget https://easylist.to/easylist/easylist.txt
3. out/Release/ruleset_converter --input_format=filter-list --output_format=unindexed-ruleset --input_files=easylist.txt --output_file=easylist_unindexed
4. out/Release/subresource_indexing_tool easylist_unindexed easylist_indexed
```

## 3. Generate the smaller filter list
```sh
1. ninja -C out/Release subresource_filter_tools
2. sh components/subresource_filter/tools/filter_many.sh 8 . out/Release/subresource_filter_tool easylist_indexed > ordered_list.txt
3. head -n 1000 ordered_list.txt | cut -d' ' -f2 > smaller_list.txt
```

## 4. Append all of the allowlist rules to be safe
Appends allowlist rules and also deduplicates rules which only differ by their set of affected domains.
```sh
1. grep ^@@ easylist.txt >> smaller_list.txt
2. sort smaller_list.txt | uniq > deduped_smaller_list.txt
3. awk -F,domain= '{ if(!length($2)) table[$1] = ""; else table[$1 FS] = length(table[$1 FS]) ? table[$1 FS] "|" $2 : $2; } END{ for (key in table) print key table[key] }' deduped_smaller_list.txt > final_list.txt
```

## 5. Turn the final list into a form usable by Chromium tools
The final filterlist has been generated. If you'd like to convert it to Chromium's binary indexed format, proceed with the following steps:

```sh
1. ninja -C out/Release/ subresource_filter_tools
2. out/Release/ruleset_converter --input_format=filter-list --output_format=unindexed-ruleset --input_files=final_list.txt --output_file=final_list_unindexed
3. out/Release/subresource_indexing_tool final_list_unindexed final_list_indexed
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
5. In your Chrome user-data-dir, go to the `Subresource Filter/Unindexed` directory. Duplicate the latest version directory and increment the number: e.g. `cp -R 9.34.0/ 9.34.1/` (note: long-term, Chrome may replace this with a real list again when a new version is found).
6. Update the `version` property in `manifext.json` to match the incremented version number
7. Overwrite `Filtering Rules` with the unindexed ruleset generated in step 3: `cp $CHROME_DIR/mock_easylist_indexed ./Filtering\ Rules`
8. Remove `manifest.fingerprint` and `\_metadata`, leaving just `Filtering Rules`, `LICENSE.txt`, and `manifest.json`: `rm -rf manifest.fingerprint _metadata`
9. Open Chrome. Confirm you're on the incremented version in chrome://components. A matching version directory should be created in `Subresource Filter/Indexed`, e.g. `Subresource Filter/Indexed Rules/35/9.34.1`

The ruleset is now loaded in Chrome but filtering will only occur on a pages
that have the ad blocker activated. Activation can be simulated using the "Force ad
blocking on this site" option in [DevTools Settings](https://www.chromium.org/testing-chrome-ad-filtering/).
