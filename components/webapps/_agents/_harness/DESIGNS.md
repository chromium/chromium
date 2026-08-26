# Technical Design Documents (Designs)

<!--
**Agent Preamble:**

> **CRITICAL:** Before reading this document or authoring a design, you MUST
> read the project's `AGENTS.md` file (if it exists) to understand the
> architecture, style, and boundaries. Also review the general coding workflow
> at `agents/prompts/common.md`.
-->

This document describes the requirements for authoring a Technical Design
Document in Chromium.

## Purpose of a Design

A Design document is strictly about **architecture**. Its purpose is to explore,
define, and document the structure, data models, API surfaces, dependencies, and
technical trade-offs of a proposed system *before* any implementation planning
or coding begins.

A Design document answers the questions:

- "What problem are we solving?"
- "What is the shape of the data, C++ classes, and Mojo APIs?"
- "What were the alternative approaches, and why were they rejected?"

**A Design Document is NOT an Execution Plan.** It does not contain
implementation steps, file editing instructions, or milestones. It focuses
entirely on establishing agreement on the technical architecture.

## Lifecycle and Workflow

In standard Chromium engineering:

1. **Design First:** The Design document is authored, adversarially reviewed
   using [REVIEWS.md](REVIEWS.md), iterated, and approved by the user/team
   *first*.
2. **Plan Second:** Only after architectural consensus is reached via the Design
   document should an Execution Plan (see [PLANS.md](PLANS.md)) be authored.

## Authoring a Design

When asked to author or propose a technical design, conduct thorough research of
the existing repository context and generate a comprehensive markdown document.
Ensure your design is objective and addresses the constraints of the system.

**Tailoring for Scale:** This template is comprehensive and covers all
considerations for a major Chrome feature. For smaller changes, refactors, or
internal-only restructurings, many sections (e.g., Enterprise, Privacy, UI,
Finch) may not apply. In such cases, do not delete the sections; instead, keep
the headings and explicitly mark them as **N/A** (Not Applicable) with a brief,
one-sentence explanation of why it is not affected (e.g., *"N/A: This is a pure
refactor with no behavioral or user-visible changes"*). This ensures all
critical areas have been consciously considered.

### File Naming and Location

All new design documents must be placed in the project's `designs/` subdirectory
(e.g., `path/to/project/_agents/designs/` or `_harness/designs/`). They must use
lowercase letters, hyphens for separation, and be prefixed with their creation
date (`YYYY-MM-DD`). They must follow the naming convention:
`YYYY-MM-DD-feature-name-design.md` (e.g.,
`2026-08-25-feature-flagging-design.md`).

### Required Structure

Your Design document must follow this format and abide by Chromium
Gitiles-flavored markdown rules (using standard relative links, no `@/` in
markdown):

```markdown
---
id: "YYYY-MM-DD-feature-name"
title: "Design: [Feature Name]"
project: "path/to/project"
author: "user@chromium.org"
status: "draft"
date: "YYYY-MM-DD"
bug: "crbug.com/1234567"
---

<!--
**Agent Preamble:**
> **CRITICAL:** Before reading this design or writing any code, you MUST read
> the project's AGENTS.md (if it exists).

**Execution Plans:**
*   `plans/YYYY-MM-DD-feature-name-plan.md` (Optional: Link to plans once created)
-->

## 1. Context and Goals

**Problem formulation:** What is the specific problem you are trying to solve?
Describe the current state and its deficiencies.

**Background:** Discuss motivation, link to mocks, screenshots, related
features, etc.

**Goals:** What are the objective requirements for a successful design?
*   Goal 1

**Non-Goals:** Explicitly state what this design will *not* attempt to solve to
prevent scope-creep.
*   Non-goal 1

## 2. Proposed Architecture

High-level architecture overview. How does this align with the subsystem
boundaries (e.g. Browser/Renderer Mojo boundary, DEPS constraints)?

### Platforms Affected
*   [ ] Windows
*   [ ] Mac
*   [ ] Linux
*   [ ] ChromeOS
*   *Android Form Factors:*
    *   [ ] Android (Smartphones/Tablets)
    *   [ ] Chrome Custom Tabs (CCT)
    *   [ ] Android WebView
*   [ ] iOS

### Process & Thread Model
*   Which Chrome processes will this code run in? (Browser, Renderer, GPU,
    Network, Utility, etc.)
*   Does it introduce new IPCs (Mojo)?
*   Does it perform work on the main threads? If so, how do we avoid blocking
    them?

### Data Models & Schemas

Detail the shape of data. Structures, database schemas, preferences, or core
domain classes.

### API Surface & Mojo Interfaces

Define public interfaces, C++ class declarations, or `.mojom` definitions.

## 3. Alternatives Considered

Explore at least one viable alternative. Describe approach and state trade-offs
(e.g. memory footprint, security sandboxing, performance).

## 4. Core Principle Considerations

### Speed & Efficiency
*   **Main Thread Impact:** Does this add work to browser/renderer main threads?
*   **Startup & Critical Paths:** Is it on the critical path of startup,
    navigation, or rendering?
*   **Memory Footprint:** Expected RAM usage (Small <=100K, Medium <1MB,
    Large >=1MB).
*   **Binary Size:** Expected impact on download/install size.
*   **Other Resources:** Disk I/O, CPU, battery, Mojo IPC frequency.
*   *AI Workloads:* (If applicable) Are there heavy/frequent AI workloads?

### Security
*   **Threat Model:** Sketch the threat model.
*   **Rule of Two:** Does it run in a sandboxed process if it parses untrusted
    data?
*   **Attack Surface:** New dependencies, IPC entry points, etc.

### Stability & Simplicity
*   Stability concerns and how they are addressed (e.g., crash keys, safe
    cleanup).
*   Simplicity of the design to avoid technical debt.

## 5. Privacy, Enterprise & A11y

### Privacy
*   Does it collect/transmit user data to Google?
*   Does it use unique/stable identifiers?
*   How does it behave in Incognito mode? (No traces on disk).
*   Does it require a Privacy Design Document (PDD)?

### Enterprise
*   Is this a breaking change for enterprises (removes API, changes UI)?
*   Do we need an Enterprise Policy to disable/configure this?

### Accessibility (A11y)
*   Does this introduce new UI?
*   Link to Greenlines/A11y Test Plan if applicable.

## 6. Metrics & Rollout Plan

### Success & Regression Metrics
*   Histograms (UMA) and User Actions to track success.
*   Metrics to monitor for regressions (performance, crashes).

### Rollout & Finch
*   Is it gated behind a `base::Feature`?
*   Finch experiment plan (platforms, stages).

## 7. Testing Plan
*   Unit tests (`unit_tests`), Browser tests (`browser_tests`).
*   Mocking strategy.
*   Startup dialog gate (`--no-first-run` if applicable).

## 8. Detailed Implementation Breakdown

Detailed architectural breakdown. Enumerate primary subsystems modified or
created. Rationale for change.

## 9. Future Work & Technical Debt
*   Existing tech debt being addressed.
*   New tech debt incurred (e.g., temporary flags) and plan to pay it down.
*   Deferred work.
```
