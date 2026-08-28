# AI Tool Definition Rules & Constraints (`AGENTS.md`)

This file governs all changes to `tools.mojom`, tool schemas, and assistant tool definitions in Chrome.

## 1. Tool Definitions are Pure API Contracts (Model-Agnostic)
* **Single-Tool Isolation:** Describe only what this specific tool does, its parameters, units/coordinate frames, and return values.
* **No Negative Peer Routing:** NEVER add negative routing constraints (e.g., *"Do not use this tool for X, use Tool Y instead"*).
* **Prompt-Agnostic Parameters:** Describe parameters by their functional identity on the active webpage (e.g., *"numeric DOM node ID of the target element (e.g. 101)"*), never referencing specific prompt headings, section titles, or formatting templates (e.g., avoid *"from the Page Content Summary"*).
* **Dynamic Availability:** Tools are conditionally injected by Chrome based on page context (e.g. video tools only when `<video>` exists). Never assume peer tools are present.
* **Public/Private Encapsulation:** Public `tools.mojom` must NEVER reference internal-only or experimental tools.

## 2. Policy & Routing Belongs in System Instructions
* If an eval or user reports that a model is choosing the wrong tool, add routing rules or few-shot examples to the assistant's system instructions—do NOT modify the tool's Mojom docstring.
* If a model is passing incomplete or ill-formatted arguments (e.g., partial text instead of full text), clarify the functional semantics in `tools.mojom`.

## 3. Formatting & Types
* **Strong Types:** Use `enum`, `integer`, or `bool` instead of open-ended string descriptions of valid values.
* **Line Wrapping:** Wrap Mojom comments within 80 columns so `generate_tool_definitions.py` produces clean JSON.
