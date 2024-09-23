## SavedTabGroups

Tab Groups are an effective way to enable users to organize their tabs, but are
ephemeral, and disappear after the user exits a session. Saved tab groups can be
 saved and recalled allowing users to keep the state of their working state
 across browsing sessions.

Saved Tab Groups are built on sync's storage layer. This storage solution works
across sessions and when sync is enabled, saved tab groups will sync across
devices updating the tabs and other metadata about the tab group in real-time.

## Testing

To run all the relevant C++ unit tests, you can run the `components_unittests`
target and give the `saved_tab_groups` filter file as an argument:

```
./out/Default/components_unittests --test-launcher-filter-file=components/saved_tab_groups/components_unittests.filter
```

To keep the list of tests updated, you must add the test group name to the
`components_unittests.filter` file whenever writing a new test file.
