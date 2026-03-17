# Jobs (`chrome/browser/web_applications/jobs`)

This directory contains reusable units of work, referred to as "jobs".

## Role of Jobs

While **[Commands](../commands/README.md)** are scheduled by the
`WebAppCommandScheduler` and orchestrate the high-level workflow of web app
operations (such as "Fetch Manifest and Install"), these workflows often share
common steps (e.g., "Install from info", "Perform an update", "Transform a
manifest into a WebAppInstallInfo", etc).

Jobs extract these common steps so they can be reused across different commands.

## Best Practices

- **Composability:** Jobs are designed to be composed together within a command
  to form complex operations.
- **No Implicit Scheduling:** Jobs are not scheduled directly by the
  `WebAppCommandScheduler`. A command must own and execute the job.
- **Clear Inputs/Outputs:** A job usually expects a clear set of inputs (often
  via its constructor) and returns a specific `JobResult` type to the owning
  command via a callback.
- **Factory Method:** Jobs should have a `CreateAndStart` static factory method
  that returns a `std::unique_ptr` to the job.
- **Lock Passing:** Jobs should accept locks as raw pointers and store them as
  `raw_ref`s. To be flexible, use the lock mixin type (like `WithAppResources`)
  instead of the concrete lock class (e.g. `AppLock`).
- **Destruction Order:** A command using jobs should delete them after they
  complete, or ensure the destruction order guarantees that the
  `raw_ref`/`raw_ptr` in the job doesn't flag a dangling pointer issue when the
  command is destroyed (since the command owns the lock).
