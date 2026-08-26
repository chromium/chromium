---
name: webapps-harness
description: >-
  Skill for loading context from the webapps harness, navigating project
  architecture, and managing rules.
---

# Skill: WebApps Harness

This skill enables AI agents to load domain context from the WebApps harness,
navigate the Central Hub and Satellite Spokes, and keep project rules up to
date.

## Context Loading

When tasked with working in the WebApps ecosystem, load context from:

- **Central Hub & Domain Rules:** [AGENTS.md](../../../AGENTS.md)
- **Spatial Map:** [CODE_STRUCTURE.md](../../CODE_STRUCTURE.md)
- **Layer Boundaries:** [DEPENDENCIES.md](../../DEPENDENCIES.md)
- **Universal Harness Standards:**
  [\_harness/AGENTS.md](../../_harness/AGENTS.md)

## Harness Maintenance & Freshness

When adding new subsystem features, refactoring directory structures, or
updating rules:

1. **Reference Front Door:** Always reference [AGENTS.md](../../../AGENTS.md).
2. **Update Spatial Maps:** If new directories or major classes were added,
   update `CODE_STRUCTURE.md` and `DEPENDENCIES.md`.
3. **Automated Maintenance:** For automated link validation and non-destructive
   rule auditing, use the `harness-updater` skill
   (`_harness/skills/harness-updater/`).
4. **Document Authoring & Review:** For writing designs or execution plans, use
   `harness-doc-writer` (`_harness/skills/harness-doc-writer/`) with
   `chromium_design_reviewer`.

## Workspace & VCS

- Use `git cl format` before committing.
- Run `git cl presubmit -u --force` to verify changes before upload.
