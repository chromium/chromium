---
name: harness-doc-writer
description: >-
  Authors and refines Technical Design Documents and Execution Plans through
  automated adversarial review. Use when asked to write or review a standalone
  design document or execution plan in a component harness. Don't use for
  orchestrating end-to-end multi-CL milestone implementation and chain review
  (use harness-project), or maintaining AGENTS.md links and harness rules (use
  harness-updater).
---

# Harness Document Writer

## Overview

This skill guides the primary project agent (e.g. `<project>_agent`) to author
high-quality Technical Design Documents (Designs) and Execution Plans (Plans)
and refine them via automated adversarial review with the
`chromium_design_reviewer` subagent.

## Document Lifecycle

1. **Design First:** In standard Chromium engineering workflows, author and
   finalize the Technical Design Document first to reach architectural
   consensus.
2. **Plan Second:** Once the design is approved, author the Execution Plan to
   break down the implementation into reviewable Gerrit CL milestones.
3. **Flexible Coupling:** For smaller refactors or self-contained tasks, design
   and execution planning may iterate concurrently.
4. **End-to-End Orchestration:** Once the design is approved, use
   [harness-project](../harness-project/SKILL.md) to drive Execution Plan
   authoring, stacked-branch milestone implementation, and Big-Picture Chain
   Review autonomously.

______________________________________________________________________

## Authoring Workflow

### Phase 1: Preparation & Drafting

1. **Identify Target Type & Template:**
   - If authoring a **Design**: Reference the
     [Technical Design template](../../DESIGNS.md).
   - If authoring an **Execution Plan**: Reference the
     [Execution Plan template](../../PLANS.md).
2. **Identify Target Paths:**
   - Determine destination project directory (e.g.,
     `<product_area>/_agents/designs/` or `plans/`).
   - Use the creation date as the ID prefix formatted as
     `YYYY-MM-DD-<shortname>-design.md` or `-plan.md` (e.g.
     `2026-08-25-feature-name-design.md`) to prevent conflicts across parallel
     branches.
3. **Draft Proposal:**
   - Author the complete markdown document adhering strictly to the template
     structure.
   - For Execution Plans, specify milestone ordering, API or conversion shim
     boundaries, and concrete verification commands rather than exhaustive code
     dumps.
   - Ensure all markdown links use standard relative paths or paths from the
     `src/` root (e.g. `/content/browser/...`), never `file://` URLs or `@/`.
   - Write prose for human readers in Simplified Technical English (ASD-STE100).
     If a Simplified Technical English skill is available, use it.
   - Use active voice, at most 25 words per descriptive sentence, at most 20
     words per procedural step, and no semicolons.

### Phase 2: Adversarial Critique Loop

1. **Invoke Design Reviewer:**
   - Spawn the `chromium_design_reviewer` subagent (or evaluate against the
     [review standards](../../REVIEWS.md)).
   - Provide the complete draft proposal to the reviewer.
2. **Review Feedback:**
   - Inspect the findings across the 6 critique dimensions:
     1. **Requirements & Scope Completeness** (Functional/non-functional
        coverage)
     2. **Security & Trust Boundaries** (Rule of 2, Mojo sanitization,
        sandboxing)
     3. **System Constraints & Platform Boundaries** (Thread safety, DEPS rules)
     4. **Architectural Gaps & Failure Modes** (Error recovery, lifetime races,
        crash safety)
     5. **Complexity & Simplicity** (Minimal abstractions, reuse of Chromium
        primitives)
     6. **Reliability, Testing, Privacy & Freshness** (Incognito/Guest
        isolation, test plans)
3. **Refine Draft:**
   - Update the draft to address blockers and important critique items.
   - Repeat the review loop if major architectural adjustments were made (up to
     3 iterations).
   - After critique rounds, prune accreted prose that is obvious to a competent
     Chromium engineer while keeping brief inline context reminders on
     cross-references for skimmers.
   - If you delegate an STE rewrite to a subagent, audit the diff to confirm no
     technical details changed.
   - After substantive edits or an STE rewrite, re-run
     `chromium_design_reviewer` on the design or plan.

### Phase 3: Human Approval & Finalization

1. **Present to Developer (or Proceed in Project Runs):**
   - Pause for developer feedback and approval when authoring a standalone
     Design or standalone Execution Plan.
   - If authoring an Execution Plan inside an approved-Design
     [harness-project](../harness-project/SKILL.md) run, proceed directly to
     milestone execution once `chromium_design_reviewer` approves.
2. **Save Document:**
   - Write the finalized document to the appropriate directory (e.g.,
     `designs/YYYY-MM-DD-feature-name-design.md` or
     `plans/YYYY-MM-DD-feature-name-plan.md`).
3. **Format:**
   - Run `git cl format` to ensure proper markdown formatting.
