# LevelDB Scopes Implementation Design

This document is the implementation plan and design for LevelDB Scopes. It serves to both document the current state and the future plans for implementing LevelDB Scopes.

See LevelDB Scopes general design doc [here](https://docs.google.com/document/d/16_igCI15Gfzb6UYqeuJTmJPrzEtawz6Y1tVOKNtYgiU/edit).

# Status / Current State

* Lock manager - Done
* Scopes - Done
* Blob / large value support - Future

Things not covered in this README:

* How iterators & scopes interact to ensure the iterator is operating on fresh data

# Vocabulary

**Scope**

A scope encompasses a group of changes that can be reverted. It is basically synonymous with a transaction, and would be used for readwrite and versionchange transactions in LevelDB. The scope has a defined list of key ranges where the changes can occur. It operates by keeping an undo task log, which is either discarded on commit, or replayed on revert. It also keeps a cleanup task log for specialty operations to happen during log cleanup.

**Task**

A task is something that is executed after a scope has been committed. There are two types of tasks (**undo/revert** tasks and **cleanup** tasks), and they are stored in the separate **log**s.

**Undo Task**

Undo tasks are used to revert a scope (that is, undo the changes that were written by the scope). An undo task is a single operation that is used to undo one or more of these changes. See the [LevelDBScopesUndoTask in `scopes_metadata.proto`](scopes_metadata.proto). Undo tasks are only executed when a scope is reverted.

**Cleanup Tasks**

Cleanup tasks are optionally executed when a scopes is cleaned up. They consist of deferred deletions (range deletions that the user doesn't need to happen right away).

**Log**

There are two task logs in a scope, the `undo task log` and the `cleanup task log`. They each have a unique key prefix so they can be iterated easily.

**Scope Number (scope#)**

Each scope has an identifier unique to the backing store that is auto-incrementing. During recovery, it is set to the minimum unused scope (see more in the recovery section).

**Sequence Number (seq#)**

Every undo log entry has a unique sequence number within the scope. These should start at {max int} (using Fixed64) and decrement.

**Commit Point**

This signifies that a scope has been committed. The commit point for a scope is the absence of `locks` in the scope's metadata.

**Key Range**

Represents a range of LevelDB keys. Every operation has a key or a key range associated with it. Sometimes key ranges are expected to be re-used or modified again by subsequent operations, and sometimes key ranges are known to be never used again.

**Lock**

To prevent reading uncommitted data, IndexedDB 'locks' objects stores when there are (readwrite) transactions operating on them.

## New LevelDB Table Data

|Purpose|Key |Format|Value (protobufs)|
|---|---|---|---|
|Metadata|metadata|`prefix0`|`LevelDBScopesMetadata`|
|Scope Metadata|scope-{scope#}|`prefix1{scope#}`|`LevelDBScopesScopeMetadata`|
|Undo log operations|log-undo-{scope#}-{seq#}|`prefix20{scope#}{seq#}`|`LevelDBScopesUndoTask`|
|Cleanup log operations|log-cleanup-{scope#}-{seq#}|`prefix21{scope#}{seq#}`|`LevelDBScopesCleanupTask`|


### Key Format

* Allow the 'user' of the scopes system to choose the key prefix (`prefix`).
* Scope # is a varint
* Sequence # is a big-endian Fixed64 (to support bytewise sorting)

See [`leveldb_scopes_coding.h`](leveldb_scopes_coding.h) for the key encoding implementation.

### Value Format

All values are protobufs, see [`scopes_metadata.proto`](scopes_metadata.proto).

# Operation Flows

## Acquiring Locks
**IndexedDB Sequence**

* Input - lock ranges & types. The lock ranges should correspond to the key ranges that will be read from or written to. The type signifies if the lock range should be requested as exclusive or shared.
* If any of the key ranges are currently locked, then wait until they are all free. This is the IDB transaction sequencing logic.
* Output - a list of locks, one per requested lock range.

## Creating & Using a Scope
**IndexedDB Sequence**

* Input - a list of locks for the scope. See [Acquiring Locks](#acquiring-locks) above
* Create the scope 
    * Use the next available scope # (and increment the next scope #)
* Enable operations on the scope
* While the total size of changes is less than X Mb, buffer them in a write batch.
    * If the scope is committed before reaching X Mb, then just commit the scope without generating an undo log.
* If the size of the buffer write batch is > X Mb, or the user needs to 'read' in the range that was just written to, then the changes must be written to disk.
  * For every operation, the scope must read the database and append the undo operation to the undo task log.
    * See the [Undo Operation Generation](#undo-operations) section below
* Deferred operations are written do the cleanup task log.
* Output - a Scope

## Committing a Scope
**IndexedDB Sequence**

* Input - a Scope
* The scope is marked as 'committed' by writing the `LevelDBScopesScopeMetadata` (at `scope-{scope#}`) to remove the `lock` key ranges. This change is flushed to disk.
* The Cleanup Sequence is signalled for cleaning up the committed scope #.
* Output - Scope is committed, and lock is released.

## Reverting a Scope
**Revert Sequence**

* Input - revert a given scope number.
* Opens a cursor to `log-undo-{scope#}-0`
    * Cursor should be at the first undo entry
    * If the scope's commit point exists (in the scope's metadata, if the locks are empty) then error - reverting a committed scope is an invalid state in this design
* Iterate through undo tasks, committing operations.
* Update the Scope's `LevelDBScopesScopeMetadata` entry (at `scope-{scope#}`) by cleaning the `locks` and setting `ignore_cleanup_tasks = true`, and flush this change to disk.
* The Cleanup Sequence is signalled for cleaning up the reverted scope #.
* Output - Scope was successfully reverted, and lock released.

## Startup & Recovery
**IndexedDB Sequence**

* Input - the database
* Reads metadata (fail for unknown version)
* Opens a cursor to scopes- & iterates to read in all scopes
    * If the scope metadata (`LevelDBScopesScopeMetadata` at `scope-{scope#}`) has `locks`, then those are considered locked
    * The maximum scope # is found and used to determine the next scope number (used in scope creation)
* Requests locks from the lock system
* Signals IndexedDB that it can start accepting operations (IndexedDB can now start running)
* For every `LevelDBScopesScopeMetadata` that has no `locks`
    * Kick off an [Undo Log Cleanup](#undo-log-cleanup) task to eventually clean this up.
* For every `LevelDBScopesScopeMetadata` that has `locks`
    * Kick off a [Reverting a Scope](#reverting-a-scope) task. This state indicates a shutdown while a revert was in progress.
* Output - nothing, done

## Undo Log Cleanup
**Cleanup & Revert Sequence**

* Input - scope #
* Open the `scope-{scope#}` metadata
  * If the commit point is not there (if the `locks` are not empty), then error.
* If the 'ignore_cleanup_tasks' value is false, then
  * Iterate through the `log-cleanup-{scope#}-` cleanup tasks and execute them (range deletes).
* Delete both the undo tasks log and the cleanup task log
* Delete the `scope-{scope#}` metadata
* Output - nothing

# Lock Manager

The scopes feature needs a fairly generic lock manager. This lock manager needs two levels, because versionchange transactions need to lock a whole database, where other transactions will lock smaller ranges within the level one range.

### API Methods

#### AcquireLocks

Accepts a list of lock request and a callback which is given a moveable-only lock object, which releases its lock on destruction. Each lock request consists of a lock type (shared or exclusive), a level number, and a key range.  The levels are totally independent from the perspective of the lock manager.

To keep the implementation simple, all required locks for a given scope or operation need to be acquired all at once.

### Internal Data Structure

The lock manager basically holds ranges, and needs to know if a new range intersects any old ranges. A good data structure for this is an Interval Tree.

# Undo Operations

Undo operations are generated when the undo tasks are required. Note that for under a certain scope 'size' (where the buffer write batch is small enough), no undo operations are generated.

## Types

* `Put(key, value)`
* `Delete(key)`
* `DeleteRange(key_range)`

See `LevelDBScopesUndoTask` in the [`scopes_metadata.proto`](scopes_metadata.proto)

## Generation

### Normal Cases

Note - all cases where "old_value" is used requires reading the current value from the database.

**`Put(key, value)`**

* `Delete(key)` if an old value doesn't exist,
* `Put(key, old_value)` if an old value does exist, or
* Nothing if the old value and new value matche

**`Add(key, value)`**

* Delete(key), trusting the client that there wasn't a value here before.

**`Delete(key)`**

* `Put(key, old_value)` if the old_value exists, or
* Nothing if no old_value exists.

**`DeleteRange(key_range)`**

* `Put(key, old_value)` for every entry in that key range. This requires iterating the database using the key_range to find all entries.

### Special Case - Empty Unique Key Range

#### Creation - key range is empty initially

If the values being created are in a key range that is initially empty (we trust the API caller here - there is no verification), and if the key range will never be reused if it is reverted, then the undo log can consist of a single:

`DeleteRange(key_range)`

Examples of this are creating new databases or indexes in a versionchange transaction. The scopes system can check to make sure it's range is empty before doing operations in debug builds.

#### Deletion - key range will never be used again.

This is done by creating a cleanup task (see `LevelDBScopesCleanupTask` in [`scopes_metadata.proto`](scopes_metadata.proto)). When the scope is cleaned up, these operations are executed. This allows a user to defer the deletion to a later time and a different thread.