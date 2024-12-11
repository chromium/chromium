# Persistent Cache

Persistent cache provides an API to store and retrieve key-value pairs in
files. It is serverless; any process/thread with access to the files can open
them via the persistent cache API and use it to store/retrieve entries.
Concurrency is managed internally by persistent cache. Entries stored in a
persistent cache may be evicted at any point, so it's incorrect to assume that
an entry that was just stored in the cache can be retrieved.

The usage pattern targeted by persistent_cache is kept simple on purpose to
allow for flexibility of implementation in the backends. Clients should expect
to be able to store and retrieve values but cannot rely on any other behavior.

## Success guarantees

persistent_cache represents at its core a best-effort mechanism. It's
impossible to guarantee an entry was inserted since the backing could run out
of space.  Instead of treating this occurrence like an out-of-memory exception
and crashing the classes in this file rely on the contracts described by their
APIs to guide user code into running the appropriate checks.

## Security

While persistent_cache is agnostic to Chrome's security model some of its
design decisions are made to comform to it.

### File access.

* Backends have to be able to function being provided only `base::File` instances and not paths.
* Backends cannot rely on the ability to create or delete files.
* Backends have to be functional even with read-only access to files.

## Eviction

To retain simplicity of design persistent_cache does not expose or enforce
eviction mechanisms.  Clients of the cache can assume that they are free to
insert as many entries are desired without having to reason about size
management.
