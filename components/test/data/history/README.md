## How to generate history.N.sql files using a Chromium build.

On a Linux build:

1. Build the `sqlite_shell` target. This will build the [SQLite CLI].

        $ ninja -C out/Debug/ sqlite_shell

2. Run Chrome/Chromium with a fresh profile directory and immediately quit. It
   doesn't really matter how long you run it, but there'll be less work for you
   if you quit early.

        $ out/Debug/chrome --user-data-dir=foo

3. Locate the `History` file in the user-data-dir directory; e.g., `foo/Default/History`.

4. Dump the `History` database into a text file:

        $ echo '.dump' | out/Debug/sqlite_shell foo/Default/History > history.sql

5. Manually remove all `INSERT INTO` statements other than the statements
   populating the `meta` table.

<!-- This section adapted from comment in history_backend_db_unittest.cc. -->
In the past, we only added a history.57.sql file to the repo while adding a
migration to the NEXT version 58. That's confusing because then the developer
has to reverse engineer what the migration for 57 was.
If you introduce a new migration, add a test for it in `HistoryBackendDBTest`,
and add a new `history.N.sql` file for the new DB layout so that
`HistoryBackendDBTest.VerifyTestSQLFileForCurrentVersionAlreadyExists` keeps
passing. SQL schemas can change without migrations, so make sure to verify the
`history.N-1.sql` is up-to-date by re-creating. The flow to create a migration
`N` should be:
1. There should already exist `history.N-1.sql`.
2. Re-create `history.N-1.sql` to make sure it hasn't changed since it was
   created.
3. Implement your migration to version `N`.
4. Add a migration test above the line labeled `NEW MIGRATION TESTS GO HERE`,
   beginning with `CreateDBVersion(n-1)` and ending with
   `ASSERT_GE(HistoryDatabase::GetCurrentVersion(), n);`
5. Create `history.N.sql`. Then run `git cl presubmit` to get a command to add
   the new file to a filelist in this directory.

[SQLite CLI]: https://www.sqlite.org/cli.html
