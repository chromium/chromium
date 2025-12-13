# Strike database

Strike databases are a way of counting "strikes" in a per-profile manner that
is persisted across restarts and, due to caching, available synchronously.
Strike information is not synced across devices and, depending on the chosen
implementation, provides support for easily clearing it on browsing data
removal.

An example use case for strikes is the address autofill: To ensure that a user
does not see too many "Save address" prompts, autofill uses a strike database
that keeps track of how often a user has declined to save a profile - it can do
so either by tracking counting consecutive declines on a given domain or by
counting how often a user has declined to save a specific address.

To use a strike database, you need to do the following:
- Define `Traits` that specify how your strike database is supposed to behave,
  e.g., the expiry time of strikes or the maximum number of strikes before a
  features is blocked.
- Create either an instance of `SimpleStrikeDatabase<YourTraits>` or
  `HistoryClearableStrikeDatabase<YourTraits>` and pass a pointer to the
  `StrikeDatabaseBase`. This is a `KeyedService` that you obtain in your
  embedder (e.g., Chrome).
- Use your strike database!

Note that `SimpleStrikeDatabase<YourTraits>` and
`HistoryClearableStrikeDatabase<YourTraits>` contain no state beyond a pointer
to the `StrikeDatabaseBase`. Therefore instantiating lots of strike databases
across multiple tabs is cheap and they all share the same underlying database.
