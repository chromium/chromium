# Execution Plans (ExecPlans)

This document describes the requirements for authoring an Execution Plan
(ExecPlan) in Chromium.

## Purpose of an Execution Plan

An Execution Plan is a **living, persistent markdown artifact** that tracks
multi-day, multi-CL projects. While a Design Document defines *what* to build
and *why*, an Execution Plan defines *how* to execute the implementation
step-by-step across reviewable Gerrit Changelists (CLs).

**Key Principles:**

1. **Milestone = CL Boundary & Reviewability:** Map each milestone to one
   focused Gerrit CL following Chromium's
   [small CL guidelines](/docs/cl_tips.md). Separate mechanical edits (such as
   moves, renames, or large fixture updates) from behavioral logic into distinct
   milestones. Prefer incremental shims over flag-day flips for wide migrations,
   and specify ordering, boundaries, and contracts instead of code dumps.
2. **Independent Compilation & Testability:** Ensure every milestone compiles
   and passes observable tests (`autoninja`, `tools/autotest.py`) across
   affected platforms. Add verification guards where needed, such as keeping
   parity coverage when replacing legacy logic or guarding files that must stay
   in sync (via lint blocks or tests).
3. **Idempotence & Explicit Scope:** Write milestone steps so agents can re-run
   them safely, and list explicit target file paths for each milestone.
4. **Living State Tracking:** Update milestone checkboxes,
   `Surprises & Discoveries`, and `Decision Log` as implementation progresses.
5. **Harness Freshness & Chain Review:** Ensure the final milestone audits
   `_agents/` freshness. When orchestrated with
   [harness-project](skills/harness-project/SKILL.md), also run its whole-chain
   review.

## Authoring an Execution Plan

Write prose for human readers in Simplified Technical English (ASD-STE100). If a
Simplified Technical English skill is available, use it.

### File Naming and Location

Place execution plans in the project's `plans/` subdirectory (for example,
`path/to/project/_agents/plans/` or `_harness/plans/`). Name each file
`YYYY-MM-DD-feature-name-plan.md` using lowercase letters and hyphens (for
example, `2026-08-25-feature-refactor-plan.md`), and link back to the parent
Design Document. Keep the `id` and `date` frontmatter fields set to this
creation date so filenames and cross-links remain stable. When `status` becomes
`"completed"`, add `completion_date: "YYYY-MM-DD"` to the frontmatter.

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

> Read the project's `AGENTS.md` (if present) before executing milestones.
> Run `git cl format` and `git cl presubmit -u --force` before completing
> each milestone. Pause for user confirmation between milestones on
> standalone plans, or proceed autonomously when running under
> `harness-project`.
> Keep `id` and `date` set to the creation date. When `status` becomes
> `"completed"`, add `completion_date: "YYYY-MM-DD"` to the frontmatter.
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
      `AGENTS.md` if architecture, layering, or guidelines shifted.

*   **Validation and Acceptance:**
    - All tests green, presubmit passes, harness updated and accurate.

## 7. Artifacts and Notes

- [Links to parent design doc, PRD, or relevant docs]

## 8. Outcomes & Retrospective

_(To be filled upon completion of the final milestone)_
```

## Review and Approval

Present standalone Execution Plans to the user for approval before starting
implementation. When orchestrating from an approved Design with
[harness-project](skills/harness-project/SKILL.md), validate the Plan with
`chromium_design_reviewer` and execute milestones autonomously. Pause for user
confirmation between milestones on standalone plans, and escalate during
orchestrated runs only on Design flaws, blockers, or final handoff.
