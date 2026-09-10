# WebApps AI Agent Harness Setup

AI agent personas, skills, and architectural context for Web Applications
(`components/webapps/`, Desktop PWAs, Android WebAPKs), built on the
[generic AI agent harness](_harness/README.md).

## How to Enable in Your Workspace

Add `"inherits"` entries for `components/webapps/_agents/agents.json` and
`components/webapps/_agents/skills.json` to your workspace root `.agents/`
configuration files.

### 1. Configure `.agents/agents.json`

```json
{
  "inherits": [
    {
      "path": "components/webapps/_agents/agents.json"
    }
  ]
  , ... your other stuff...
}
```

Run from `src/`:

```bash
PRODUCT_AREA_DIR="components/webapps"
command -v jq >/dev/null 2>&1 || { echo "Error: jq is required but not installed." >&2; false; } && {
  mkdir -p .agents
  [ -s .agents/agents.json ] || echo "{}" > .agents/agents.json
  jq --arg p "${PRODUCT_AREA_DIR%/}/_agents/agents.json" \
    'if (.inherits // []) | any(.path? == $p) then . else .inherits += [{"path": $p}] end' \
    .agents/agents.json > .agents/agents.json.tmp && mv .agents/agents.json.tmp .agents/agents.json
}
```

### 2. Configure `.agents/skills.json`

```json
{
  "inherits": [
    {
      "path": "components/webapps/_agents/skills.json"
    }
  ]
  , ... your other stuff...
}
```

Run from `src/`:

```bash
PRODUCT_AREA_DIR="components/webapps"
command -v jq >/dev/null 2>&1 || { echo "Error: jq is required but not installed." >&2; false; } && {
  mkdir -p .agents
  [ -s .agents/skills.json ] || echo "{}" > .agents/skills.json
  jq --arg p "${PRODUCT_AREA_DIR%/}/_agents/skills.json" \
    'if (.inherits // []) | any(.path? == $p) then . else .inherits += [{"path": $p}] end' \
    .agents/skills.json > .agents/skills.json.tmp && mv .agents/skills.json.tmp .agents/skills.json
}
```
