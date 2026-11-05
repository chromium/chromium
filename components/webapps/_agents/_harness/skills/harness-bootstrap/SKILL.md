---
name: harness-bootstrap
description: >-
  Bootstraps a minimal skeleton AI Agent Harness (`_agents/` directory and
  hub/spoke `AGENTS.md` files) for a Chromium project or team. Use when asked
  to bootstrap, scaffold, or set up a harness for a new team or component.
---

# Harness Bootstrap

Scaffold a bare-bones AI Agent Harness for a Chromium product area or team.

See [the README.md](../../README.md) for the general intention, goals, and
non-goals of this harness framework.

## Core Principle: Skeleton Only

Start as small as possible:

- **No invented rules or gotchas:** Do not scan the codebase to generate coding
  rules, style guidelines, testing rules, or architectural invariants unless the
  operator explicitly instructs you to include specific ones.
- **Directory mapping & existing docs only:** The initial `AGENTS.md` files
  exist only to map out which directories belong to the product area, briefly
  state what each directory is for (1-2 sentences), and link to existing
  `README.md` or documentation files.
- **Grow on demand:** Rules, testing recipes, and gotchas are added later by the
  team (using `harness-updater`) when agents actually miss context or make
  repeated mistakes in practice.

## 1. Gather Project Scope from the Operator

Before creating files, confirm the following with the operator (or extract from
their prompt and confirm):

1. **Hub directory (`<HUB_DIR>`):** The central directory for the product area
   relative to `src/` (e.g. `components/autofill`).
2. **Project shortname (`<project>`):** Short lowercase identifier used for the
   skill (`<project>-dev`) and agent (`<project>_agent`, using underscores for
   the agent name and hyphens for the skill name).
3. **Spoke directories:** Other directories across Chromium that are part of
   this product area (e.g. `chrome/browser/...`, `chrome/browser/ui/...`,
   `content/browser/...`, `third_party/blink/...`).
   - If the operator isn't sure of all spoke paths, you can search for candidate
     directories (by directory name or `OWNERS` references) and ask the operator
     which ones should get spoke `AGENTS.md` files.
4. **Optional internal directory (`internal/agents/teams/<project>/`):** If the
   workspace has an `internal/` (`src-internal`) checkout, ask whether the team
   wants a skeleton internal directory created and inherited by the public
   manifests so private Chromium skills/agents or `google3/` team context can be
   added there.
5. **Optional additions:** Whether the operator has any existing skills, google3
   manifests, or specific initial rules they explicitly want wired in now.
   Default to none.

## 2. Inspect Directories for Existing Docs

For `<HUB_DIR>` and each confirmed spoke directory:

- Check for existing documentation files (`README.md`, `SECURITY.md`, `docs/`).
- Read just enough (`README.md` or top-level files) to write a concise 1-2
  sentence description of what each directory is responsible for.
- If a directory already has an `AGENTS.md`, inspect it first and ask the
  operator before overwriting or merging.

## 3. Generate the Skeleton Files

Create the following files (and nothing else unless requested):

### 1. Hub `AGENTS.md` (`<HUB_DIR>/AGENTS.md`)

Contains a 1-2 sentence overview of the product area and an ecosystem map of the
hub and spoke directories leading with a bold 2-4 word architectural role title
followed by the directory path and links to `AGENTS.md` and `README.md` (when
present). Omit rules/gotchas sections unless the operator asked for specific
items.

```markdown
# <Project Title> Central Hub (<HUB_DIR>)

<1-2 sentence summary of the product area in Chromium.>

## Ecosystem Map (Directories & Spokes)

- **<2-4 Word Subsystem Role>** (`<HUB_DIR>/`): [README.md](README.md)
  <1-sentence summary of what lives here.>
- **<2-4 Word Subsystem Role>** (`<SPOKE_DIR>/`):
  [AGENTS.md](/<SPOKE_DIR>/AGENTS.md) · [README.md](/<SPOKE_DIR>/README.md)
  <1-sentence summary of what lives here.>
```

*(Only include `[README.md](...)` or other doc links when those files actually
exist on disk. Use repo-root-relative `/path/to/...` links for directories
outside `<HUB_DIR>`.)*

### 2. Spoke `AGENTS.md` files (`<SPOKE_DIR>/AGENTS.md`)

Create a minimal spoke file in each confirmed satellite directory pointing back
to the hub:

```markdown
# <Spoke Title> (`<SPOKE_DIR>`)

**Parent:** [<Project Title> Central Hub](/<HUB_DIR>/AGENTS.md)

<1-2 sentence summary of what this directory is responsible for.>

## Canonical Docs

- [README.md](README.md)
```

*(Omit `## Canonical Docs` if the spoke has no `README.md` or existing docs.)*

### 3. `<HUB_DIR>/_agents/skills/<project>-dev/SKILL.md`

A bare-bones development skill that links to the hub `AGENTS.md` and notes the
shared harness skills:

```markdown
---
name: <project>-dev
description: >-
  How to develop, build, and test <Project Title> features in Chromium.
---

# <Project Title> Development

Supplementary engineering information for the <Project Title> product area.

[Central Hub & Directory Map](/<HUB_DIR>/AGENTS.md)

## Design & Review Workflows

- For non-trivial designs or execution plans, use `harness-doc-writer` and
  coordinate reviews with `chromium_design_reviewer`.
- After structural changes or new directories, audit links and maps with
  `harness-updater`.
```

### 4. `<HUB_DIR>/_agents/agents/<project>_agent.md`

The project agent persona that deterministically loads the hub `AGENTS.md` and
`<project>-dev` skill:

```markdown
---
name: <project>_agent
description: >-
  Agent for development, testing, and general assistance in the <Project Title>
  project space.
tools:
  - run_command
  - write_to_file
  - replace_file_content
  - view_file
  - list_dir
  - find_by_name
  - grep_search
  - code_search
  - moma_search
  - invoke_subagent
  - send_message
  - manage_subagents
  - manage_task
  - ask_question
  - schedule
  - read_url_content
  - search_web
  - notebook_edit
inheritMcp: true
mainAgent: true
subagent: true
rules:
  - ../skills/<project>-dev/SKILL.md
  - ../../AGENTS.md
---

# <Project Title> Agent

Act as a chromium engineer and assist development, testing, and architecture
across <Project Title>.
```

### 5. `<HUB_DIR>/_agents/skills.json` and `<HUB_DIR>/_agents/agents.json`

Use repository-root-relative paths (from `src/`):

`<HUB_DIR>/_agents/skills.json`:

```json
{
  "inherits": [
    {
      "path": "components/webapps/_agents/_harness/skills.json"
    }
  ],
  "entries": [
    {
      "path": "<HUB_DIR>/_agents/skills/<project>-dev"
    }
  ]
}
```

`<HUB_DIR>/_agents/agents.json`:

```json
{
  "inherits": [
    {
      "path": "components/webapps/_agents/_harness/agents.json"
    }
  ],
  "entries": [
    {
      "path": "<HUB_DIR>/_agents/agents"
    }
  ]
}
```

### 6. `<HUB_DIR>/_agents/README.md`

Create a setup README with a `# <Project Title> AI Agent Harness Setup` heading,
a 1-sentence intro linking to
[the generic AI agent harness](/components/webapps/_agents/_harness/README.md),
and the workspace enablement instructions from
[the harness README.md](../../README.md)
(`"Adding" a project to your AI config`), substituting `<HUB_DIR>` for
`<PRODUCT_AREA_DIR>` so team members can copy-paste the `jq` commands to add
`<HUB_DIR>/_agents/agents.json` and `<HUB_DIR>/_agents/skills.json` to their
`.agents/` manifests.

### 7. (Optional) Internal Team Skeleton (`internal/agents/teams/<project>/`)

If the operator opted to create an internal directory for private Chromium
skills/agents or `google3/` context inheritance:

1. Add `"inherits"` entries in the public `<HUB_DIR>/_agents/skills.json` and
   `<HUB_DIR>/_agents/agents.json` pointing to
   `"internal/agents/teams/<project>/skills.json"` and
   `"internal/agents/teams/<project>/agents.json"`. (Missing `inherits` paths
   are silently ignored on external checkouts without `src-internal`.)
2. Create the skeleton in `internal/agents/teams/<project>/` (note that
   `internal/` is a separate git repo and will be uploaded as its own CL):
   - `OWNERS`: Team owners (e.g. copied or referenced from `<HUB_DIR>/OWNERS`).
   - `README.md`: Brief note that this directory holds internal-only team
     skills/agents and/or inherits `google3/` team manifests, and is
     automatically inherited by `<HUB_DIR>/_agents/*.json`.
   - `skills.json`:
     ```json
     {
       "entries": [
         {
           "path": "internal/agents/teams/<project>/skills"
         }
       ]
     }
     ```
   - `agents.json`:
     ```json
     {
       "entries": [
         {
           "path": "internal/agents/teams/<project>/agents"
         }
       ]
     }
     ```
   *(If the team also has a `google3/` agent manifest path, add it under
   `"inherits"` in those internal JSON files. Missing `entries` directories are
   silently ignored by Jetski until the first internal skill or agent is checked
   in.)*

## 4. Verify & Offer Workspace Setup

1. Verify every markdown link and public manifest path resolves to an existing
   file on disk.
2. Run `git cl format` on the new markdown and JSON files.
3. Ask the operator if they would like you to run the `.agents/agents.json` and
   `.agents/skills.json` `jq` commands in their current workspace so the new
   harness is enabled locally.
