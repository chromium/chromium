# Quarantined AI Agent Harness Infrastructure

## Overview

This directory contains the generic, subsystem-agnostic AI Agent Harness
infrastructure for Chromium.

It is currently **quarantined** within `components/webapps/_agents/_harness/` as
a pilot implementation. By keeping reusable templates, personas, universal
guidelines (`AGENTS.md`), and skills self-contained here, initial changes avoid
modifying the shared repository root while remaining structurally identical for
future promotion.

## Directory Structure

```
_harness/
├── README.md                 # This file
├── AGENTS.md                 # Universal guidelines (C++, Mojo, Testing)
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

- Standardize a way to structure agent documentation.
- Standardize a place for a project/team to put their suggested agents & skills
  that are useful for developing that project.
- Facilitate project-specific agent (backed by a project skill) that contains
  pointers to all relevant context for a project in Chromium.
- Facilitate the design - plan - implement agentic development model.

## How to Use

Put an `"inherits"` entry for your project in your `.agents/agents.json` (or
`_agents/agents.json`) file. Here is an example for the 'webapps' team:

```json
{
  "inherits": [
    {
      "path": "components/webapps/_agents/_harness/agents.json"
    }
  ]
  , ... your other stuff...
}
```

And put an `"inherits"` entry for your project in your `.agents/skills.json` (or
`_agents/skills.json`) file. Here is an example for the 'webapps' team:

```json
{
  "inherits": [
    {
      "path": "components/webapps/_agents/_harness/skills.json"
    }
  ]
  , ... your other stuff...
}
```

## Developing

When modifying the harness or a project-specific harness:

- It is important to keep tokens to a minimum, so be as minimal as possible.
- Do not add context or rules to the AGENTS.md file unless they are consistently
  broken or missed. This is intended to supplement the smarts of the agent.
  Rules that are created by AI without examples of mistakes are always going to
  be unnecessary, as the AI was able to come up with this.

## Promotion Roadmap

Once this harness model is vetted in `components/webapps/` and adopted across
additional subsystems (e.g. `components/autofill/`, `components/omnibox/`), this
directory will be promoted to the repository root:

1. Move contents of `components/webapps/_agents/_harness/` to repo-root
   `_agents/` (with `_harness/AGENTS.md` promoted to
   `{workspace_root}/_agents/AGENTS.md`).
2. Update project `AGENTS.md` and manifests (`agents.json`, `skills.json`) to
   inherit from `_agents/`.
3. Introduce the `harness-bootstrap` scaffolder skill and central
   `HARNESS_INDEX.md`.
