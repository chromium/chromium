---
name: harness-doc-writer
description: >-
  Workflow for authoring Technical Design Documents and Execution Plans with
  automated adversarial review. Use when asked to "write a design", "author an
  execution plan", or create documentation in any component harness.
---

# Harness Document Writer

## Overview

This skill guides the primary project agent (e.g. `webapps_agent`) to author
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

______________________________________________________________________

## Authoring Workflow

### Phase 1: Preparation & Drafting

1. **Identify Target Type & Template:**
   - If authoring a **Design**: Reference
     `components/webapps/_agents/_harness/DESIGNS.md`.
   - If authoring an **Execution Plan**: Reference
     `components/webapps/_agents/_harness/PLANS.md`.
2. **Identify Target Paths:**
   - Determine destination project directory (e.g.,
     `components/webapps/_agents/designs/` or `plans/`).
   - Use the creation date as the ID prefix formatted as
     `YYYY-MM-DD-<shortname>.md` (e.g. `2026-08-25-feature-name.md`) to prevent
     conflicts across parallel branches.
3. **Draft Proposal:**
   - Author the complete markdown document adhering strictly to the template
     structure.
   - Ensure all markdown links use standard relative paths (never `@/`).

### Phase 2: Adversarial Critique Loop

1. **Invoke Design Reviewer:**
   - Spawn the `chromium_design_reviewer` subagent (or evaluate against
     `components/webapps/_agents/_harness/REVIEWS.md`).
   - Provide the complete draft proposal to the reviewer.
2. **Review Feedback:**
   - Inspect the findings table across the 6 critique dimensions:
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

### Phase 3: Human Approval & Finalization

1. **Present to Developer:**
   - Share the refined proposal and critique summary with the developer for
     feedback or approval.
2. **Save Document:**
   - Write the finalized document to the appropriate directory (e.g.,
     `designs/YYYY-MM-DD-feature-name.md`).
3. **Format:**
   - Run `git cl format` to ensure proper markdown formatting.
