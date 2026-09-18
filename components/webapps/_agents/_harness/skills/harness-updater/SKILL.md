---
name: harness-updater
description: >-
  Maintains an AI Agent Harness (`_agents/` directory and `AGENTS.md` files):
  repairs links and manifests, and routes new rules or procedures to the
  correct file. Guides where to put requested additions in the harness.
---

# Harness Updater

Maintain an AI Agent Harness: the `_agents/` tree and the `AGENTS.md` files
scattered through a product area.

See [the README.md](../../README.md) file for the general intention / goals /
non-goals of this harness framework.

## 1. Automatic updates / maintenance

Using the specifications & information in the above readme, look for absent
links (e.g. an AGENTS.md or README.md file exists but it's not linked in a
directory listing that includes that directory) or broken links (directory
doesn't exist anymore, or file doesn't, etc).

## 2. Requested updates by the user

Using the specifications & information in the above readme, help guide the user
about where to put the information they are requesting to be added to the
harness.

Sometimes, if the user is saying an agent keeps doing X, it can be helpful to
ask "What information may the agent have not had that would have made the better
choice obvious?". You could do some research here, and if that is not fruitful,
usually this is a good question to ask the user. As the smallest change here is
often just documenting a relevant resource, caveat, or known problem, etc.

When adding rules & gotchas, having the 'why', a bad example, and a good example
is effective.

## 3. Size reporting & guidance

AGENTS.md size guidance limits: 10kb for a hub, 5kb for a spoke, and anything
approaching 24kb starts to be automatically truncated.

```bash
git ls-files '*/AGENTS.md' | xargs wc -c | sort -n
```

## General documentation guidance

Sometimes things shouldn't go into AGENTS.md files. These locations can also be
considered:

A README.md file or other existing (or a new one can be proposed) markdown
documentation files:

- How different code systems work together.
- How classes in a system work together.

Class documentation or documentation in a header file:

- Class responsibilities, and/or invariants
- Usage instructions for class.

## Also

- Avoid Markdown tables — they don't survive `git cl format` cleanly.
- No ASCII art in AGENTS.md files. Bullets and Mermaid are fine.
- Encourage against rules that do what `clang-format`, `checkdeps`, presubmit,
  or the compiler already enforce. Removing an existing rule of that kind can be
  proposed (but confirmation required).
