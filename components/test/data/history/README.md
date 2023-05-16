## How to generate history.N.sql files using a Chromium build.

On a Linux build:

1. Build the `sqlite_shell` target. This will build the [SQLite CLI].

        $ ninja -C out/Debug/ sqlite_shell

2. Run Chrome/Chromium with a fresh profile directory and immediately quit. It
   doesn't really matter how long you run it, but there'll be less work for you
   if you quit early.

        $ out/Debug/chrome-wrapper --user-data-dir=foo

3. Locate the `History` file in the user-data-dir directory; e.g., `foo/Default/History`.

4. Dump the `History` database into a text file:

        $ echo '.dump' | out/Debug/sqlite_shell foo/Default/History > history.sql

5. Manually remove all `INSERT INTO` statements other than the statements
   populating the `meta` table.

To generate history.N.sql, be sure to run these steps before implementing the N+1 migration in 
chromium. I.e. you'll usually want to run these steps on the main branch rather than the CL branch.

[SQLite CLI]: https://www.sqlite.org/cli.html

