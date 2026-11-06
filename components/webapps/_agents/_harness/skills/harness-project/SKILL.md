---
name: harness-project
description: >-
  Orchestrates multi-CL Chromium projects end-to-end across Design, Execution
  Plan, stacked-branch Milestone implementation, and holistic Big-Picture Chain
  Review. Use when driving a feature, refactor, or migration from problem
  framing or an approved design through a reviewable stacked CL chain. Don't
  use for single-CL fixes, standalone document drafting without execution
  orchestration (use harness-doc-writer), or maintaining AGENTS.md links and
  harness rules (use harness-updater).
---

# Harness Project Orchestrator

## Orchestration Model

Upfront Design approval unlocks autonomous planning, stacked-branch
implementation, and chain review. Keep planning and audits separate from edits:

1. **Explore:** Use a read-only subagent (or direct investigation) before
   locking assumptions.
2. **Implement:** Delegate milestone coding, builds, and tests to a
   write-enabled subagent, or the primary agent if none is available.
3. **Audit:** Run `chromium_design_reviewer` on Designs and Plans, and
   `chromium_code_reviewer` on diffs and the stack. Route findings back to the
   implementer rather than editing inside a reviewer.

## Phase 1: Collaborative Design & Human Gate

If invoked with an approved Design, skip to Phase 2. Otherwise, collaborate with
the human:

1. **Ground the Problem:** Anchor problem statements in concrete architectural
   defects, such as representable invalid states or repeated manual fixes.
2. **Verify Assumptions:** Use a read-only subagent (or direct investigation) to
   verify non-obvious IPC, threading, or platform assumptions.
3. **Draft and Critique:** Follow [DESIGNS.md](../../DESIGNS.md) and
   [harness-doc-writer](../harness-doc-writer/SKILL.md) to draft the Design, run
   adversarial critique with `chromium_design_reviewer`, and prune obvious prose
   while keeping brief context on cross-references.
4. **Human Gate:** Stop for human review (and Gerrit review if uploaded) until
   approved.

## Phase 2: Autonomous Execution Planning

Once the Design is approved, author the Plan using the
[Execution Plan template](../../PLANS.md) and the
[harness-doc-writer](../harness-doc-writer/SKILL.md) workflow. Skip human review
unless the user asked for a Plan check. Have `chromium_design_reviewer` audit
the Plan against the **Testability** and **Reviewability** principles in the
template, and iterate until approved. Return to Phase 1 if planning exposes a
Design flaw.

## Phase 3: Inner Loop: Serial Stacked-Branch Execution

Execute dependent milestones serially so each builds on a verified parent:

1. **Branch:** Branch Milestone 1 from `origin/main` (or the parent Design or
   Plan branch), and track each child branch via
   `git branch --set-upstream-to=<parent-branch>`.
2. **Implement:** Have a write-enabled subagent (or the primary agent if none is
   available) write code and tests, build (`autoninja`), run
   `tools/autotest.py --run-all`, and format (`git cl format`). Stage explicit
   file paths rather than `git add -u` or `git commit -a`.
3. **Commit & Presubmit:** Commit staged paths following the project's CL title
   and description conventions (see the project's dev skill or `AGENTS.md`),
   then run `git cl presubmit -u --force`.
4. **Escalate Stalls:** If an implementer stalls twice on the same obstacle, try
   a stronger model or take over directly.
5. **Code Review:** Run `chromium_code_reviewer` on the committed `HEAD` diff
   against the Design and Plan, routing fixes back to the implementer until
   approved.
6. **Update Plan:** Update checkboxes, `Surprises & Discoveries`, and
   `Decision Log` after each milestone.

If a milestone hits structural friction or breaks a planned boundary, jump to
Phase 4 instead of improvising.

## Phase 4: Outer Loop: Big-Picture Chain Review

Trigger this audit when all milestones finish or when a milestone hits a
structural snag. Run `chromium_code_reviewer` at the chain tip to inspect
`git log` and `git diff` across stacked branches (optionally paired with
`chromium_design_reviewer`):

1. **Design Fit:** Does the Design hold against code reality, edge cases, memory
   footprint, and binary size?
2. **Testability & Guards:** Does each milestone compile and pass tests across
   platforms, guard synced files (such as via lint blocks or tests), and
   preserve earlier fixes?
3. **Reviewability & Splits:** Can mixed CLs split into smaller mechanical and
   behavioral CLs while staying compilable? Do CL titles and descriptions follow
   project conventions (see the project's dev skill or `AGENTS.md`)?
4. **Documentation:** Are code comments, `README.md`, and `AGENTS.md` accurate
   and concise?

Continue vs. Stop rules:

1. **Continue Autonomously:** When review recommends Plan edits, CL splits,
   guards, or doc fixes preserving the Design, update the Plan, amend or split
   branches, update `--set-upstream-to`, cascade-rebase children, re-run Phase 3
   on affected milestones, and re-audit.
2. **Stop and Escalate:** Stop and present options to the human if code reality
   invalidates the Design, exposes a major scope trade-off, hits an unresolvable
   toolchain blocker, or needs human authority (such as Gerrit replies or
   `CQ+2`).
3. **Stop and Hand Off:** Once the chain passes review, stop and present a
   sitrep of the verified branches for Gerrit upload.

## Phase 5: Gerrit Iteration, Rolling Rebases & Closeout

Keep Gerrit reviews fast and prevent rebase regressions:

1. **Pre-Upload Gate, Waves & Drafts:** Run a pre-upload read-only review before
   uploading patchsets. Send large chains in bottom-up waves by `OWNERS`
   boundary. Verify drafted Gerrit replies against diffs and leave them as
   drafts unless asked to publish.
2. **Preserve Votes:** Amend commits in-place or group minor owner cleanups into
   a tip CL to keep `Code-Review +1` votes intact.
3. **Rolling Rebases:** When prompted as bottom CLs land (avoid unprompted
   `git fetch`), reparent the next open branch onto `origin/main` and run
   `gclient sync -D` once on that base branch. Cascade-rebase children onto
   updated parents, re-verify builds and tests, and diff tip refactors against
   their parent CL to preserve earlier fixes.
4. **Closeout:** Set Plan frontmatter to `status: completed` with
   `Outcomes & Retrospective`, and Design frontmatter to `status: implemented`
   with a proportional retrospective. Run
   [harness-updater](../harness-updater/SKILL.md) to verify links and freshness.
