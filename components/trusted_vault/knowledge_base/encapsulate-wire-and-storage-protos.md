---
name: trusted_vault/encapsulate-wire-and-storage-protos
rubric: design
severity: should_fix
---

**Rule.** Do not expose wire or storage `trusted_vault_pb::*` protobuf messages
in public APIs consumed outside `//components/trusted_vault` (such as
`TrustedVaultClient`, `TrustedVaultService`, `StandaloneTrustedVaultClient`, or
`StandaloneTrustedVaultFrontend`), and keep `//components/trusted_vault/proto`
as an internal `deps` rather than `public_deps` of
`//components/trusted_vault:trusted_vault`. Using `trusted_vault_pb::*` messages
internally within standalone trusted vault classes (such as between backend,
connection, recovery factors, and storage) is fine.

**Why.** Exposing wire or storage protobuf messages in public component APIs
leaks internal protocol and persistence schemas to external consumers across
`//components` and `//chrome`, whereas keeping `trusted_vault_pb::*` internal to
the standalone implementation preserves a clean component boundary.

