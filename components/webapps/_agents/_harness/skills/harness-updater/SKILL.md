---
name: harness-updater
description: >-
  Skill for auditing link integrity, verifying rule freshness, validating
  harness manifests, and updating spatial maps (CODE_STRUCTURE.md,
  DEPENDENCIES.md) upon completing features.
---

# Harness Updater

Use this skill to audit, validate, and maintain an AI Agent Harness (`_agents/`
directory and `AGENTS.md`) across any Chromium component.

## Core Architecture

An AI Agent Harness organizes project intelligence into distinct files. Per the
updated Jetski guidelines (see `agent-rules.md` and `agent-custom-agents.md`),
all rules are consolidated into standard `AGENTS.md` files (no frontmatter), and
personas use the preferred Markdown format (`.md`). Because Chromium formatting
(`git cl format`) strictly wraps lines at 80 characters, **file size (bytes /
tokens)** is the authoritative budget gate, with line counts serving as
approximate guidance:

| File                            | Purpose                            | Budget (File Size)   |
| :------------------------------ | :--------------------------------- | :------------------- |
| **`AGENTS.md`**                 | Entrypoint, spoke router, rules    | ≤3 KB (~60–80 lines) |
| **`_harness/AGENTS.md`**        | Universal standards & router       | ≤2 KB (~40–50 lines) |
| **`_agents/CODE_STRUCTURE.md`** | Directory map & subsystem pointers | ≤3 KB (~60–80 lines) |
| **`_agents/DEPENDENCIES.md`**   | Dependency guidance & boundaries   | ≤2 KB (~50 lines)    |

______________________________________________________________________

## Guiding Principles

### 1. Behavioral Contract, Not Documentation Store

`AGENTS.md` and `_agents/` files are a **behavioral contract and rulebook, not
documentation**.

- **Human READMEs** own all narrative architecture, class relationships, and
  system overviews.
- **Agent files** tell the model *what to do*, *what never to do*, *where to
  find rules*, and *where to place new code*.
- Do not describe how classes work or duplicate README prose in agent files.

### 2. Include Only What Cannot Be Inferred

Only include information the agent cannot deduce from reading headers, BUILD.gn,
or existing documentation:

- Do not list classes, methods, or internal subsystem inventories.
- Focus strictly on non-obvious project conventions, safety boundaries, and
  subsystem placement rules.

### 3. Consolidated Single Source of Truth (`AGENTS.md`)

Each fact and canonical doc link must appear in **exactly one file**:

- **Subsystem-level rules:** Consolidated into the nearest `AGENTS.md` file.
  Storing separate rule files in `rules/*.md` is deprecated.
- **Universal standards:** Referenced from `_harness/AGENTS.md`.
- **Project-level doc links:** Owned exclusively by `AGENTS.md`.

### 4. Keep Eager Context Under 200 Lines

The walk-up chain (`AGENTS.md` $\\to$ `CODE_STRUCTURE.md` $\\to$
`DEPENDENCIES.md`) must stay within **≤200 lines / ≤10 KB combined** to prevent
context window saturation and reasoning degradation.

### 5. Lazy-Load Templates

- **Template files** (`DESIGNS.md`, `PLANS.md`, `REVIEWS.md`) must only be
  loaded by their specific workflow skills (`harness-doc-writer`) or agents
  (`chromium_design_reviewer`), never on the general coding path.

### 6. Avoid ASCII Art in Agent Files

LLMs process text sequentially and experience "spatial blindness" with complex
ASCII diagrams. Use concise text lists or Mermaid instead. Keep visual ASCII
diagrams in human READMEs.

### 7. Add Rules Reactively, Not Preemptively

- Do not write speculative rules for general Chromium conventions the model
  already knows.
- Add rules **reactively** when agents or reviewers observe repeated mistakes in
  code reviews.
- If a rule can be enforced by a compiler, linter (`clang-format`), DEPS
  checker, or CI test, let the tool handle it.

### 8. Spoke AGENTS.md Files Are Minimal

Satellite spoke `AGENTS.md` files in subdirectories should contain only:

1. A link back to the Central Hub `AGENTS.md`.
2. A **1–2 sentence elevator pitch** describing the primary purpose of the
   directory for instant semantic orientation.
3. A link to the local subsystem `README.md`.
4. Truly unique local constraints and domain rules (if any) not covered by
   parent rules.

______________________________________________________________________

### 9. Precedence & Override Hierarchy

Order of precedence across the context chain:

1. **User Chat Prompt** (highest precedence — explicit user instructions
   override everything)
2. **Local Spoke `AGENTS.md`** (subsystem-specific conventions override hub
   defaults)
3. **Central Hub `AGENTS.md`** (project-level conventions)
4. **Base Harness Standards (`_harness/AGENTS.md`)** (lowest precedence —
   fallback standards)

Specific instructions from the user or local directory rules always override
general hub conventions.

______________________________________________________________________

## File Specifications

### CODE_STRUCTURE.md (Directory Map)

- **Purpose:** Maps physical directories to subsystems with links to human docs.
- **Content:**
  1. Subsystem directory path + 1-sentence purpose.
  2. Link to authoritative subsystem `README.md`.
  3. No inline class inventories, header listings, or placement tables.

### DEPENDENCIES.md (Dependency Guidance)

- **Purpose:** Informs agents of vertical layer boundaries and DEPS policies.
- **Content:**
  1. Link to canonical layering docs.
  2. Concise vertical layer list (≤10 lines).
  3. Core invariants (3–5 bullet points).
  4. Encouraged / Discouraged / Banned dependency lists.

### AGENTS.md (Consolidated Domain Rules & Router)

- **Purpose:** Primary entrypoint, spoke routing, and consolidated domain
  guardrails.
- **Content:**
  1. Subsystem elevator pitch & canonical doc links.
  2. Satellite spoke routing links.
  3. Architectural invariants and non-obvious domain rules.
  4. Testing commands and guardrails.
  5. Primary agent & skill registrations.
- **Rule Authoring Standards:**
  - **Paired Examples:** Show both `// Correct:` and `// Incorrect:` snippets
    where appropriate.
  - **Clarification Triggers:** State explicitly when agents must pause and ask
    the human for confirmation.
  - **Off-Limits Boundaries:** List generated files (e.g. `*_jni.h`) or
    sensitive directories that agents must not edit directly.
  - **Done Criteria:** Define verification steps (compile target, test runner
    command, format check).

______________________________________________________________________

## Anti-Pattern Checklist

When auditing a harness, check for and fix these patterns:

| Anti-Pattern                 | Detection                                         | Fix                                                             |
| :--------------------------- | :------------------------------------------------ | :-------------------------------------------------------------- |
| **Deprecated rule files**    | Separate `rules/*.md` files with YAML frontmatter | Consolidate Markdown rules into `AGENTS.md`.                    |
| **Legacy JSON agents**       | `agent.json` + `config.yaml` agent definitions    | Migrate to single Markdown agent file (`.md`).                  |
| **Documentation store**      | `_agents/` file describes how code works          | Replace with link to human `README.md`.                         |
| **Duplicate link blocks**    | Same doc URL in 2+ `_agents/` files               | Assign each link to exactly one file.                           |
| **Rule re-inlining**         | Spoke AGENTS.md repeats hub rules                 | Keep spoke minimal (elevator pitch + unique local constraints). |
| **Eager template loading**   | DESIGNS/PLANS appear in main prompt               | Reference only from skills/agent configs.                       |
| **Oversized spatial maps**   | CODE_STRUCTURE > 3 KB or DEPENDENCIES > 2 KB      | Trim inline class descriptions. Link to README.                 |
| **Common knowledge rules**   | Rule states general C++/Chromium conventions      | Remove unless repeated mistakes are observed.                   |
| **Linter-enforced rules**    | Rule duplicates `.clang-format` or CI checks      | Let the tool handle it; remove from agent rules.                |
| **ASCII art in agent files** | Text diagrams in `_agents/` files                 | Replace with concise text list or Mermaid.                      |

______________________________________________________________________

## Maintenance Workflow

### 1. Link & Structural Integrity Audit

1. **Verify Markdown Links:** Ensure all links resolve to valid files on disk:
   - Use standard relative links (`./file.md`, `../sibling.md`) only for local
     navigation within 1–2 levels.
   - Use repository-root-relative links (e.g. `/components/webapps/AGENTS.md`)
     for distant or cross-subsystem links; avoid deep `../../../../..` chains.
   - Linkify descriptive document/entity names directly instead of duplicating
     raw path text.
2. **Verify Registry Manifests:** Ensure paths in `agents.json` and
   `skills.json` are repository-relative from `src/`.

### 2. Updating Spatial Maps & Policies

- **When adding new directories:** Update `CODE_STRUCTURE.md` with a 1-line
  entry and link to its README.
- **When modifying DEPS:** Update `DEPENDENCIES.md` under Encouraged /
  Discouraged / Banned.
- **When capturing recurring reviewer feedback:** Add or update domain rules
  directly in the relevant `AGENTS.md`.

### 3. Token Budget Verification

After any update, verify file sizes:

```bash
wc -c AGENTS.md _agents/CODE_STRUCTURE.md \
  _agents/DEPENDENCIES.md _agents/_harness/AGENTS.md
```

- `AGENTS.md` ≤ 3 KB
- `_harness/AGENTS.md` ≤ 2 KB
- `CODE_STRUCTURE.md` ≤ 3 KB
- `DEPENDENCIES.md` ≤ 2 KB
