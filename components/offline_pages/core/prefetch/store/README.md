# Using offline prefetch store

This document provides background for understanding and prescribes how to
interact with the offline prefetch store.

[TOC]

## Simple interface

There are 2 goals of the `PrefetchStoreSQL` class:

* Provide a way to run a group of related commands in a transaction, to achieve
  atomicity.
* Remove the burden of switching threads between foreground and background from
  the caller. (All SQL store interactions are actually running on the background
  thread, but that fact is transparent to the caller.)

Therefore the store offers the following interface:

```cpp
class PrefetchStore {
 public:
  // Definition of the callback that is going to run the core of the command in
  // the |Execute| method.
  template <typename T>
  using RunCallback = base::OnceCallback<T(sql::Database*)>;

  // Definition of the callback used to pass the result back to the caller of
  // |Execute| method.
  template <typename T>
  using ResultCallback = base::OnceCallback<void(T)>;

  // Creates an instance of |PrefetchStore| with an in-memory SQLite database.
  explicit PrefetchStore(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  // Creates an instance of |PrefetchStore| with a SQLite database stored in
  // |database_dir|.
  PrefetchStore(scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
                const base::FilePath& database_dir);

  ~PrefetchStore();

  // Executes a |run_callback| on SQL store on the blocking thread, and posts
  // its result back to calling thread through |result_callback|.
  // Calling |Execute| when store is NOT_INITIALIZED will cause the store
  // initialization to start.
  // Store initialization status needs to be SUCCESS for |run_callback| to run.
  // If initialization fails, |result_callback| is invoked with |default_value|.
  template <typename T>
  void Execute(RunCallback<T> run_callback,
               ResultCallback<T> result_callback,
               T default_value);

  // Gets the initialization status of the store.
  InitializationStatus initialization_status() const;
};
```

It allows for enough flexibility to execute `run_callback` on a background
(blocking) task runner with a `sql::Database` pointer provided, and then
return result using `result_callback`.

## How to implement your command

Running code against prefetch store requires defining 2 functions. First one
responsible for working with the store on the background thread, while second to
deliver results to the foreground.

### Defining a RunCallback

Signature of the callback to run on a background thread is defined by
`PrefetchStoreSQL::RunCallback`. Basic implementation should contain the
following (in this example `bool` is used as a sample return type.

```cpp
bool ExampleRunCallback(sql::Database* db) {
  // Note that the run function won't be called if the database fails to
  // initialize, so db is always non-null.

  // It's best to start a transaction, which can be aborted without making
  // changes to the data. This is one of the reasons to go with provided store
  // interface.
  sql::Transaction transaction(db);

  // If transaction does not begin, we can leave. This is another error
  // condition that should not be CHECK/DCHECKed, as failure to open transaction
  // happens and should be handled gracefully by the caller.
  if (!transaction.Begin())
    return false;

  // Code doing the work goes here.

  // Because we are running in transaction code, whenever we run into an error,
  // we can simply return and *transaction will be aborted*.
  if (/* stuff went wrong */)
    return false;

  // If everything went well up to this point, we attempt to commit a
  // transaction. If the attempt fails, we return false again and transaction
  // will be aborted for us.
  if (!transaction.Commit())
    return false;

  // Everything went right, transaction is committed at this point. We should
  // return appropriate result without attempting any more work with the DB.
  // It should be OK to report UMA if necessary.
  return true;
}
```

### Defining a ResultCallback

Return of the RunCallback will be provided to ResultCallback using a move
semantics. This is how result is made available on the foreground thread.
ResultCallback takes over ownership of the passed in result.

If the database fails to initialize, the RunCallback isn't called and instead,
the default_value provided to Execute is passed to the ResultCallback.

```cpp
void ExampleResultCallback(bool success) {
  if (!success) {
    // Log error, UMA the problem or issue a retry.
    LOG(ERROR) << "DB operation failed. All hope is lost.";
    return;
  }

  // Do regular follow up work.
}
```

In the simple example above, we are using `bool` as a return type, but it would
be much better to use an enum, or a structure containing an enum, that gives a
better detail of what went wrong.
See `//components/offline_pages/core/prefetch/add_unique_urls_task.cc` for a
good example of working consumer of the API.

## Column ordering in table creation

Column ordering is important to simplify data retrieval in SQLite 3 tables. When
creating a table fixed-width columns should be declared first and variable-width
ones later.

Furthermore, when adding or removing columns, any existing column ordering might
not be kept. This means that any query must not presume column ordering and must
always explicitly refer to them by name. Using <code>SELECT * FROM ...</code>
for obtaining data in all columns is therefore *unsafe and forbidden*.

## Schema History / Test Data

The components/test/data/offline_pages/prefetch/version_schemas directory contains
data used in testing the prefetch database schema,
see prefetch_store_schema_unittest.cc for the tests. In this directory, there
are two files for every version of the prefetch database schema:

- v#.sql

SQL that creates the database schema for this version and inserts rows in each
table for testing. This defines the initial state for each migration test. Data
inserted here should attempt to cover edge cases specific to that version (like
a change in a default value).

- v#.data

Represents the expected result of running the initial state defined in the .sql
file through the migration logic up to the current version of the schema.
Or in pseudo-code:

 migrated_db = MigrateToCurrentSchema(BuildDbFromSqlFile(old_version_sql_file));
 EXPECT_EQ(GetDataFromDataFile(old_version_data_file),
    GetDataFromDb(migrated_db));

Whenever a new version is created, existing .data might need to be updated to
account for the added migration step.
