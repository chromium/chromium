---
name: enterprise/keep-knowledge-base-rules-atomic-and-scoped
rubric: design
severity: should_fix
---

**Rule.** When authoring or updating `knowledge_base/*.md` rules for
`agentic_review`, enforce three invariants:
1. **One atomic concept per file:** Each `.md` file MUST cover a single,
   cohesive invariant. Never stitch together unrelated review comments into a
   multi-topic rule.
2. **Directory-aligned scope:** Place subsystem-specific rules in the narrowest
   owning directory (e.g. `chrome/browser/glic/gemini_enterprise/knowledge_base/`),
   and keep shared rules (e.g. `components/enterprise/knowledge_base/`) strictly
   cross-cutting across all directories importing that `COMMON_METADATA` mixin.
3. **Verified codebase symbols:** Verify all C++, `.mojom`, and `IDS_*` symbols
   and method signatures in `Bad`/`Good` examples against the real Chromium
   codebase (`origin/main`); never invent non-existent identifiers or conflict
   with valid codebase exceptions.

**Why.** Multi-topic "spaghetti" rules, overly broad `COMMON_METADATA` mixins,
and unverified or fictional API examples cause the automated code review agent
to emit noisy false positives and suggest code that does not compile.

## Examples

Bad:

```markdown
---
name: enterprise/features-delegates-and-cert-renewal
rubric: design
severity: should_fix
---

**Rule.** Default `BASE_FEATURE` flags to `FEATURE_DISABLED_BY_DEFAULT`, make
all delegate methods pure virtual, and check `expected_cn` in `CertProvisioner`.
```

Good:

```markdown
---
name: enterprise/respect-keyed-service-shutdown
rubric: safety
severity: should_fix
---

**Rule.** In `KeyedService` subclasses (such as `CloudProfileReportingService`)
that hold `base::ScopedObservation`s, timers, controllers, or sub-schedulers
referencing `Profile`, `CloudPolicyCore`, or other `KeyedService`s, override
`Shutdown()` to reset those members during two-phase shutdown before the
destructor runs.
```
