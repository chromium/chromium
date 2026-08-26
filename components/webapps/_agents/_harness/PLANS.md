# Execution Plans (ExecPlans)

<!--
**Agent Preamble:**

> **CRITICAL:** Before executing any milestone, you MUST read the project's
> `AGENTS.md` (if it exists) to understand operating procedures. Run
> `git cl format` and `git cl presubmit -u --force` before completing each
> milestone. Proceed to next milestone only after user confirmation.
-->

This document describes the requirements for authoring an Execution Plan
(ExecPlan) in Chromium.

## Purpose of an Execution Plan

An Execution Plan is a **living, persistent markdown artifact** that tracks
multi-day, multi-CL projects. While a Design Document defines *what* to build
and *why*, an Execution Plan defines *how* to execute the implementation
step-by-step across reviewable Gerrit Changelists (CLs).

**Key Principles:**

1. **Milestone = CL Boundary:** Each milestone MUST correspond to exactly ONE
   Gerrit CL. Follow Chromium's small CL guidelines (`docs/cl_tips.md`).
2. **Observable Verification:** Every milestone MUST list explicit build
   commands (`autoninja`) and test commands (`tools/autotest.py` or test
   binaries) with expected outcomes.
3. **Idempotence:** Steps should be safe to re-run without breaking workspace
   state.
4. **Living State Tracking:** Keep the check-boxes up to date as milestones
   complete. Document surprises, bugs, and architectural decisions discovered
   along the way.
5. **Harness Freshness Audit:** The final milestone must explicitly audit and
   update `_agents/` (e.g., `CODE_STRUCTURE.md`, `DEPENDENCIES.md`, `AGENTS.md`)
   to reflect any architectural shifts made during development.

## Authoring an Execution Plan

### File Naming and Location

All execution plans must be placed in the project's `plans/` subdirectory (e.g.,
`path/to/project/_agents/plans/` or `_harness/plans/`). They must use lowercase
letters, hyphens for separation, and be prefixed with their creation date
(`YYYY-MM-DD`). They must follow the naming convention:
`YYYY-MM-DD-feature-name-plan.md` (e.g., `2026-08-25-feature-refactor-plan.md`),
and link back to their parent Design Document.

### Required Structure

```markdown
---
id: "YYYY-MM-DD-feature-name"
title: "Plan: [Feature Name]"
project: "path/to/project"
author: "user@chromium.org"
status: "in-progress"
date: "YYYY-MM-DD"
design_doc: "../designs/YYYY-MM-DD-feature-design.md"
bug: "crbug.com/1234567"
---

<!--
**Agent Preamble:**
> **CRITICAL:** Run git cl format and git cl presubmit before completing each
> milestone.
-->

## 1. Purpose / Big Picture

Explain in a few sentences what the user gains after this change and how they
can see it working. State the user-visible behavior enabled.

## 2. Context and Orientation

Describe current state relevant to this task. Reference key files by full
Chromium path. Define any non-obvious terms.

## 3. Progress

- [ ] **Milestone 1: [Name]**
- [ ] **Milestone 2: [Name]**
- [ ] **Milestone 3: Verification & Harness Freshness**

## 4. Surprises & Discoveries

Document unexpected behaviors, optimizations, or bugs discovered during
implementation.

## 5. Decision Log

Record decisions made while working on the plan, including rationale and date.

## 6. Plan of Work (Milestones)

### Milestone 1: [Milestone Name]

*   **Concrete Steps:**
    - Edit files: `path/to/project/browser/path/to/file.cc`
    - Build command: `autoninja -C out/Default unit_tests`
      *(FYI: check out/*/args.gn for existing build directories when targeting
      non-default platforms)*
    - Test command:
      `tools/autotest.py -C out/Default path/to/project/browser/path/to/file_unittest.cc`
*   **Interfaces and Dependencies:**
    - [List classes, functions, or Mojo interfaces created or modified]
    - [Check browser/DEPS or renderer/DEPS boundaries]
*   **Validation and Acceptance:**
    - [State what to observe to confirm the milestone is successful]
    - [Verify code formatting with: git cl format]
    - [Verify presubmit checks with: git cl presubmit -u --force]

### Milestone 2: [Milestone Name]

*   **Concrete Steps:**
    - [Implementation steps...]
*   **Validation and Acceptance:**
    - [Test commands and verification...]

### Milestone 3: Verification & Harness Freshness

*   **Concrete Steps:**
    - Format all files with `git cl format`.
    - Run full target test suite with `tools/autotest.py` or `autoninja`.
    - Run presubmit checks with `git cl presubmit -u --force`.
    - **Harness Freshness Audit:** Inspect code changes and update
      `_agents/CODE_STRUCTURE.md`, `_agents/DEPENDENCIES.md`, or `_agents/rules/`
      if architecture, boundaries, or guidelines shifted.
*   **Validation and Acceptance:**
    - All tests green, presubmit passes, harness updated and accurate.

## 7. Artifacts and Notes

- [Links to parent design doc, PRD, or relevant docs]

## 8. Outcomes & Retrospective

_(To be filled upon completion of the final milestone)_
```

## Review and Approval

Once an Execution Plan is authored, it must be presented to the user for
approval before executing Milestone 1.
