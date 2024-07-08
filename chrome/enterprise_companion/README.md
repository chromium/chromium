# Chrome Enterprise Companion App

Chrome Enterprise Companion App (CECA) is an elevated daemon for MacOS and
Windows that handles the enterprise device management features which are not
feasible to implement in browser.

The mission of CECA is to empower development of client-side enterprise features
and reduce complexity by decoupling enterprise functionality from the updater
client.

## Design

### Event Logging

CECA collects service-related data and transmits that to a remote logging service
via the EventLogger interface. The collected data is represented by the proto
files in //chrome/enterprise_companion/proto.

To support batching of request and rate-limiting, logs recorded by individual
EventLogger instances are flushed to an EventLoggerManager. Flushing occurs
either when the EventLogger is destroyed or when requested explicitly.

The manager handles the serialization and transmission of log events to the
remote endpoint. If an EventLogger flushes while the manager is rate-limited,
the logs will be queued and transmitted as soon as the manager is able.

The EventLogger interface is intended to record both the start and stop of
operations. Calling a logging method indicates the start of an operation while
invoking the returned callback signifies the end.

The EventLoggerManager will wait 15 minutes between requests. Alternatively, the
logging service may respond with timeout, in which case the larger of the two
values is used.
