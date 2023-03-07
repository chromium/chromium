The memory-system component provides a common interface to initialize memory
components like GWP-ASan, which must be enabled early in the startup of each
embedder that supports them.

Furthermore it acts as a holder for various memory components that must exist
throughout the embedders lifetime.
