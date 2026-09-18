# Quarantined AI Agent Harness Infrastructure

## Overview

This directory contains the generic, subsystem-agnostic AI Agent Harness
infrastructure for Chromium. The purpose of this harness is to:

1. Provide supplemental information to agents that is not inherently possible by
   the agent alone. Or, would require a lot of searching (and thus context
   wasting) to find.
2. Provide a basic design / plan / implement / review model to aid in
   ai-assisted development that span multiple conversations and CLs.

The harness provides a per-project structure:

- A location to document all directories / code locations relevant to a product
  area that is easily available to agents, and a way to find this from those
  code locations.
- A location to put supplemental information for developing in a product area
  (like non-obvious testing procedures, project-specific procedures, etc).
- A project agent that automatically loads the above context.

The harness provides shared:

- Templates for designs, plans, and reviews
- Basic doc writing skill, and doc reviewing skill & agent.
- Basic code reviewing agent.
- Project harness updater skill.

Finally, this provides a place to put all skills & agents a team member of this
project should have included for agentic work in their area (in the
`_agents/skills.json` and `_agents/agents.json` files).

The harness is currently **quarantined** within
`components/webapps/_agents/_harness/` as a pilot implementation. By keeping
reusable templates, personas, and skills self-contained here, initial changes
avoid modifying the shared repository root while remaining structurally
identical for future promotion.

## Directory Structure

```
_harness/
├── README.md                 # This file
├── DESIGNS.md                # Standard design doc template
├── PLANS.md                  # Standard execution plan template
├── REVIEWS.md                # Review spec & critique dimensions
├── agents.json               # Manifest registering harness personas
├── agents/                   # Reusable review personas (Markdown)
│   ├── chromium_code_reviewer.md # Read-only code reviewer persona
│   └── chromium_design_reviewer.md # Skeptical Architect reviewer
├── skills.json               # Manifest registering harness skills
├── skills/                   # Reusable harness skills
│   ├── harness-doc-writer/   # Design/Plan authoring & review loop
│   └── harness-updater/      # Link audit & freshness maintenance
├── designs/                  # Harness's own design documentation
│   └── 2026-08-25-webapps-ai-harness-mvp-design.md
└── plans/                    # Harness's own execution plans
    └── 2026-08-25-webapps-ai-harness-mvp-plan.md
```

## Goals

- Standardize a way to structure supplementary agent documentation.
- Standardize a place for a project/team to put custom or recommended
  skills/agents that all team members should have available.
- Facilitate project-specific agent (backed by a project skill) that contains
  pointers to all relevant context for a project in Chromium.
- Facilitate the design - plan - implement agentic development model.

## Non-Goals

- Exhaustively document how to write code or tests in Chromium, or for that
  product area.
- Document rules, methods, procedures, etc that the model can already inherently
  know or do.

## How to Use

Initially, the harness for a project should be extremely minimal.

- AGENTS.md files:
  - No 'rules' or 'gotchas' yet.
  - Simple directory mappings with links to the relevant agent/README.md/docs.
- <project>-dev Skill:
  - Simply links to the central AGENTS.md file.
- <project>\_agent agent:
  - Auto-includes the skill and AGENTS.md file as rules, and gives basic framing
    for the product area.

The 'harness-updater' skill attempts to help with changes to the harness. It
will automatically attempt to do things like fix broken links, but it not
intended to suggest other changes (as the fact that it suggests them means they
usually don't need to be in there). However, when users determine information
should be in the project harness, it can help suggest where it should go.

The harness should only be added to if agents repeatedly miss key information or
make a mistake. In this case, try to imagine what information or context they
did not have, and ask the agent to use the harness updater skill to figure out
what should be changed to help. Think of everything here as *supplemental*
information.

### AGENTS.md files

Each project has a 'hub' AGENTS.md file, and then relevant directories can have
a 'spoke' AGENTS.md file that points back to the hub.

- 1-2 sentence summary.
- (spoke-only) Link to the 'hub' AGENTS.md file.
- Code architecture map of relevant directories, a brief summary of their
  responsibility, and a link to the AGENTS.md file (if exists) and README.md
  file (if exists)
  - This is usually hub-only, but it's totally ok for a spoke directory with its
    own subsystem folders to have this section. Especially if those subsystems
    aren't really relevant to the main hub project.
- A list of supplemental rules / gotchas / future plans for this directory /
  subsystem.
  - Note: Product-area wide rules/gotchas should usually live in the product
    area skill.

While not all agents eagerly load these, they are very visible to agents, and
should be kept as short and succinct as possible.

### \<project>-dev SKILL.md

Each project has a development skill. This is the place to put supplemental
information specific to the whole project or development in this product area.
As mentioned above, this is *supplemental* information, so this likely will
start blank for a project. This is a good place to put common testing gotchas,
non-obvious project styles, procedures, etc.

### \<project>\_agent Agent

Each project has an agent, which automatically loads the hub AGENTS.md file and
\<project>-dev skill into context. It can also provide a place to provide some
minimal framing context if desired. The main goal here is that this agent
deterministically loads the project context, allowing for easy development or
reviewing immediately.

### `_agents/skills.json`

While listing the \<project>-dev skill, this also provides a place for the team
or TL to put skills they want ALL team members to have access to.

### `_agents/agents.json`

Similarly, while listing the \<project>\_agent, this provides a place for the
team or TL to put agents they want ALL team members to have access to.

### "Adding" a project to your AI config

To enable a project's harness in your workspace, add `"inherits"` entries for
`<PRODUCT_AREA_DIR>/_agents/agents.json` and
`<PRODUCT_AREA_DIR>/_agents/skills.json` to your workspace root `.agents/`
configuration files.

#### 1. Configure `.agents/agents.json`

Put an `"inherits"` entry for your project in your `.agents/agents.json` file:

```json
{
  "inherits": [
    {
      "path": "<PRODUCT_AREA_DIR>/_agents/agents.json"
    }
  ]
  , ... your other stuff...
}
```

Run from `src/`:

```bash
PRODUCT_AREA_DIR="<path from src/ to product area here>"
command -v jq >/dev/null 2>&1 || { echo "Error: jq is required but not installed." >&2; false; } && {
  mkdir -p .agents
  [ -s .agents/agents.json ] || echo "{}" > .agents/agents.json
  jq --arg p "${PRODUCT_AREA_DIR%/}/_agents/agents.json" \
    'if (.inherits // []) | any(.path? == $p) then . else .inherits += [{"path": $p}] end' \
    .agents/agents.json > .agents/agents.json.tmp && mv .agents/agents.json.tmp .agents/agents.json
}
```

#### 2. Configure `.agents/skills.json`

Put an `"inherits"` entry for your project in your `.agents/skills.json` file:

```json
{
  "inherits": [
    {
      "path": "<PRODUCT_AREA_DIR>/_agents/skills.json"
    }
  ]
  , ... your other stuff...
}
```

Run from `src/`:

```bash
PRODUCT_AREA_DIR="<path from src/ to product area here>"
command -v jq >/dev/null 2>&1 || { echo "Error: jq is required but not installed." >&2; false; } && {
  mkdir -p .agents
  [ -s .agents/skills.json ] || echo "{}" > .agents/skills.json
  jq --arg p "${PRODUCT_AREA_DIR%/}/_agents/skills.json" \
    'if (.inherits // []) | any(.path? == $p) then . else .inherits += [{"path": $p}] end' \
    .agents/skills.json > .agents/skills.json.tmp && mv .agents/skills.json.tmp .agents/skills.json
}
```

## Future Considerations

- Rules and information might become unnecessary without us knowing! Eval
  processes are likely required to know when removing rules is OK, but this is
  hard and would have to be per-project. For future thinking.

## Promotion Roadmap

Once this harness model is vetted in `components/webapps/` and adopted across
additional subsystems (e.g. `components/autofill/`, `components/omnibox/`), this
directory will be promoted to the repository root:

1. Move contents of `components/webapps/_agents/_harness/` to repo-root
   `_agents/`.
2. Update project `AGENTS.md` and manifests (`agents.json`, `skills.json`) to
   inherit from `_agents/`.
3. Introduce the `harness-bootstrap` scaffolder skill and central
   `HARNESS_INDEX.md`.
