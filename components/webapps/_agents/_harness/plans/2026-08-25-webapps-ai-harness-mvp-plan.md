---
id: 2026-08-25-webapps-ai-harness-mvp
title: 'Plan: WebApps AI Agent Harness MVP'
project: components/webapps
author: dmurph@chromium.org, AI Assistant
status: in-progress
date: '2026-08-25'
design_doc: ../designs/2026-08-25-webapps-ai-harness-mvp-design.md
bug: crbug.com/545323515
---

<!--
**Agent Preamble:**
> **CRITICAL:** Before executing any milestone, you MUST read the project's
> `AGENTS.md` (if it exists) to understand operating procedures. Run
> `git cl format` and `git cl presubmit -u --force` before completing each
> milestone. Proceed to next milestone only after user confirmation.
-->

# Execution Plan: WebApps AI Agent Harness MVP

## 1. Purpose / Big Picture

Establish a self-contained, component-grounded AI Harness for the Web
Applications subsystem in `components/webapps/_agents/`.

When completed, developers and AI agents can:

1. Run `webapps_agent` with instant zero-config grounding in WebApps
   architecture, DEPS boundaries, and test fakes.
2. Author new technical designs and execution plans via `harness-doc-writer`
   with automated adversarial review by `chromium_design_reviewer`.
3. Perform read-only code diff reviews with `chromium_code_reviewer`.
4. Maintain harness freshness and link integrity via `harness-updater`.
5. Keep root `agents/` clean and untouched until the harness is ready for
   repository-wide promotion.

## 2. Context and Orientation

- **Current Repository State:** A two-location prototype (commits `9862771`,
  `70ac6cbd`) placed universal templates and skills in root `agents/` and pilot
  files in `components/webapps/_agents/`.
- **Target State:** Consolidate all harness infrastructure into
  `components/webapps/_agents/`, nesting the reusable generic layer in
  `components/webapps/_agents/_harness/` and removing prototype additions from
  root `agents/`.
- **Key Constraints:**
  - **Milestone vs CL Scope for MVP:** While general execution planning targets
    1 Gerrit CL per milestone, for this foundational MVP refactor, Milestones 1
    through 4 represent internal development checkpoints executed together to
    produce **a single self-contained, reviewable Gerrit CL against
    `origin/main`** (leaving root `agents/` pristine). Subsequent feature plans
    will follow the 1-milestone-to-1-CL boundary.
  - All markdown files (`.md`) must use standard relative path links.
  - The `@/` path syntax is reserved strictly for agent configuration files
    (`config.yaml`, `agent.json`, `skills.json`).
  - Pre-existing root files (`agents/prompts/common.md`,
    `agents/prompts/templates/{desktop,android,git_operations}.md`) are
    preserved and referenced directly.

## 3. Progress

- [x] **Milestone 1: Check in Foundational Docs, Scaffold `_harness/`, &
  De-Prototype Root `agents/`**
- [x] **Milestone 2: Rewire WebApps Project Layer to Inherit from `_harness/`**
- [x] **Milestone 3: WebApps Documentation Inventory & Canonical Citation Pass**
- [x] **Milestone 4: Verification, Freshness Audit & Presubmit Acceptance**

______________________________________________________________________

## 4. Surprises & Discoveries

_(Document unexpected behaviors, optimizations, or bugs discovered during
implementation)_

______________________________________________________________________

## 5. Decision Log

- **2026-08-17:** Consolidated universal harness infrastructure into
  `components/webapps/_agents/_harness/` to keep root `agents/` pristine for
  MVP.
- **2026-08-17:** Eliminated standalone `planner` persona; `webapps_agent`
  drafts documents directly and delegates review to `chromium_design_reviewer`.
- **2026-08-17:** Replaced verbose `mojo_security.md` with thin `mojo.md` citing
  `docs/security/mojo.md`.
- **2026-08-17:** Deferred `harness-bootstrap` skill and `HARNESS_INDEX.md`
  catalog to follow-up milestones.
- **2026-08-18:** Adopted end-to-end-testable vertical slice ("steel thread")
  milestone guidance for execution planning.
- **2026-08-18:** Aligned lifecycle with SDLC (Design -> Plan -> Implement):
  Technical Design (`2026-08-25-webapps-ai-harness-mvp-design.md`) is authored,
  reviewed, and finalized *first*; the execution plan is formed subsequently as
  a separate step to structure implementation milestones (while keeping the
  workflow flexible for tightly coupled or smaller refactors).
- **2026-08-18:** Scoped MVP refactor as a single unified CL against
  `origin/main` across 4 development milestone checkpoints to avoid intermediate
  broken states.
- **2026-08-20:** Token Optimization & Index Elimination: Eliminated redundant
  `INDEX.md` and `GETTING_STARTED.md` files in favor of direct `AGENTS.md`
  entrypoint; slimmed `CODE_STRUCTURE.md` and `DEPENDENCIES.md` to thin pointers
  linking directly to authoritative human READMEs; added 1–2 sentence elevator
  pitches to satellite spoke `AGENTS.md` files; and enriched `harness-updater`
  skill with token budgets and anti-pattern checklists.
- **2026-08-24:** Modernized Harness to Updated Guidelines: Migrated legacy
  `agent.json` + `config.yaml` personas to single Markdown files (`.md`) with
  YAML frontmatter (per `agent-custom-agents.md`); eliminated deprecated
  `rules/*.md` subdirectories and `RULES.md` routers in favor of consolidated
  `AGENTS.md` files per subsystem and `_harness/AGENTS.md` for universal
  Chromium guidelines (per `agent-rules.md`).

______________________________________________________________________

## 6. Plan of Work (Milestones)

### Milestone 1: Foundational Docs, `_harness/` & Root Clean-up

**Goal:** Check in the foundational Design and Plan documents, establish the
complete `_harness/` reusable infrastructure, and remove all prototype additions
from root `agents/`.

- **Concrete Steps:**
  1. Create directory `components/webapps/_agents/_harness/`.
  2. Check in foundational architecture docs:
     - `_harness/designs/2026-08-25-webapps-ai-harness-mvp-design.md`
     - `_harness/plans/2026-08-25-webapps-ai-harness-mvp-plan.md`
  3. Create `_harness/README.md` documenting infrastructure purpose and
     promotion roadmap.
  4. Create `_harness/AGENTS.md` consolidating universal Chromium guidelines
     (C++ style, Mojo security, Testing standards).
  5. Create `_harness/DESIGNS.md`, `_harness/PLANS.md`, and
     `_harness/REVIEWS.md` standard templates.
  6. Create persona definitions in preferred Markdown format (`.md`):
     - `_harness/agents/chromium_code_reviewer.md`
     - `_harness/agents/chromium_design_reviewer.md`
  7. Create skill definitions with compliant YAML frontmatter:
     - `_harness/skills/harness-doc-writer/` (`SKILL.md`)
     - `_harness/skills/harness-updater/` (`SKILL.md`)
  8. Restore root `agents/` to pristine upstream state, dropping all prototype
     additions:
     ```bash
     git checkout origin/main -- agents/
     ```
- **Interfaces and Dependencies:**
  - `_harness/AGENTS.md` references pre-existing templates and guidelines via
    relative paths.
  - Personas and skills use relative references within `_harness/`.
- **Validation and Acceptance:**
  - `git diff --stat origin/main -- agents/` returns empty (verifying root
    `agents/` is untouched relative to `origin/main`).
  - Zero broken relative links across all newly checked-in markdown files.
  - All JSON manifests (`agents.json`, `skills.json`) pass linting:
    ```bash
    python3 -m json.tool \
      components/webapps/_agents/_harness/agents.json > /dev/null
    python3 -m json.tool \
      components/webapps/_agents/_harness/skills.json > /dev/null
    ```

______________________________________________________________________

### Milestone 2: Rewire WebApps Project Layer to Inherit from `_harness/`

**Goal:** Configure `components/webapps/_agents/` to inherit from `_harness/`
and expose project personas and skills.

- **Concrete Steps:**
  1. Update `components/webapps/AGENTS.md` to link to
     `_agents/CODE_STRUCTURE.md`, `_agents/DEPENDENCIES.md`, and
     `_agents/_harness/AGENTS.md`.
  2. Configure `components/webapps/AGENTS.md` and `_harness/AGENTS.md` as direct
     routing entrypoints and consolidated rulebooks (eliminating redundant
     `RULES.md` and `rules/*.md` files).
  3. Update `_agents/agents.json` to inherit `_harness/agents.json` and register
     `_agents/agents` using repository-relative paths.
  4. Update `_agents/skills.json` to inherit `_harness/skills.json` and register
     `webapps-harness` using repository-relative paths.
  5. Author `_agents/agents/webapps_agent.md` with project directives and skill
     pointers in preferred Markdown format.
  6. Update `_agents/skills/webapps-harness/SKILL.md` to load project context
     and reference `harness-updater`.
  7. Update all satellite spoke routing files to point to the Central Hub
     (`components/webapps/AGENTS.md`):
     - `chrome/browser/web_applications/AGENTS.md`
     - `chrome/browser/ui/web_applications/AGENTS.md`
     - `chrome/browser/ui/views/web_apps/AGENTS.md`
     - `content/browser/manifest/AGENTS.md`
     - `third_party/blink/renderer/modules/manifest/AGENTS.md`
- **Interfaces and Dependencies:**
  - `webapps_agent` inherits from `_harness/agents/` and accesses
    `_harness/skills/`.
- **Validation and Acceptance:**
  - Native walk-up from `components/webapps/` or any satellite spoke correctly
    resolves `AGENTS.md`.
  - All JSON registries contain valid repository-relative paths:
    ```bash
    python3 -m json.tool components/webapps/_agents/agents.json > /dev/null
    python3 -m json.tool components/webapps/_agents/skills.json > /dev/null
    ```

______________________________________________________________________

### Milestone 3: WebApps Documentation Inventory & Canonical Citation Pass

**Goal:** Inventory all WebApps documentation and ensure domain rules cite
canonical sources rather than duplicating knowledge.

- **Concrete Steps:**
  1. Run research pass across `components/webapps/`,
     `chrome/browser/web_applications/`, and
     `third_party/blink/renderer/modules/manifest/`.
  2. Update `_agents/CODE_STRUCTURE.md` with links to canonical subsystem
     READMEs and `BUILD.gn` targets.
  3. Update `_agents/DEPENDENCIES.md` with macro architectural boundaries and
     DEPS constraints.
  4. Consolidate Desktop PWA lock idioms and callback safety rules into
     `chrome/browser/web_applications/AGENTS.md`.
  5. Consolidate test fake guidance and test commands into
     `components/webapps/AGENTS.md` and
     `chrome/browser/web_applications/AGENTS.md`.
  6. Sweep all `.md` files in `components/webapps/_agents/` to replace any
     legacy `@/path` notation with standard relative links.
- **Interfaces and Dependencies:**
  - Direct links to `docs/` and `components/webapps/README.md`.
- **Validation and Acceptance:**
  - Scripted existence check confirms every referenced documentation path exists
    on disk.
  - Zero `@/` links remaining in any `.md` markdown files.
  - Rule files contain zero copy-pasted prose from canonical docs.

______________________________________________________________________

### Milestone 4: Verification, Freshness Audit & Presubmit Acceptance

**Goal:** Final repository-level verification, formatting, and presubmit
validation.

- **Concrete Steps:**
  1. Execute `harness-updater` link integrity audit across
     `components/webapps/_agents/**`.
  2. Run `git cl format` to format all Markdown and JSON files:
     ```bash
     git cl format --js --python
     ```
  3. Run `git cl presubmit -u --force` to ensure all repository presubmit checks
     pass:
     ```bash
     git cl presubmit -u --force
     ```
  4. Prepare final CL commit message with required tags (`TAG=agy`, `CONV=<id>`,
     `Bug=545323515`).
- **Interfaces and Dependencies:**
  - Clean Git working tree.
- **Validation and Acceptance:**
  - `git cl presubmit -u --force` passes with zero warnings or errors.
  - `git diff --stat origin/main -- agents/` confirms root `agents/` is
    completely untouched.
  - Zero broken links in markdown files and zero invalid paths in configuration
    files.

______________________________________________________________________

## 7. Artifacts and Notes

- **Design Document:**
  `_harness/designs/2026-08-25-webapps-ai-harness-mvp-design.md`
- **Execution Plan:** `_harness/plans/2026-08-25-webapps-ai-harness-mvp-plan.md`
- **Proposal Reference:** `webapps_harness_mvp_proposal.md`

## 8. Outcomes & Retrospective

The WebApps AI Agent Harness MVP successfully establishes a quarantined,
subsystem-agnostic harness foundation in `components/webapps/_agents/_harness/`.
All manifests, skills, rule dispatchers, and satellite spokes across Desktop,
Android, and Blink are wired and verified with strict zero-pollution of root
`agents/`. Ready for promotion roadmap upon broader adoption.
