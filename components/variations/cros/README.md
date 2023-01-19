# `evaluate_seed`

The executable `evaluate_seed` is a minimal program (budget: 1MiB of disk space
for the executable itself) used early in ChromeOS boot to determine which group
each early-boot experiment should be in, as well as any parameters for the
experiment. It lives here so that it is trivial to keep the code in sync between
ChromeOS's platform layer and chrome.

It will be executed primarily by `featured`, which lives in
`//platform2/featured/`.

`featured` will set command-line parameters as appropriate (e.g. to determine
enterprise enrollment state and whether to use a safe seed), and will feed a
safe seed (if necessary) via stdin.

`evaluate_seed` will write a serialized version of the computed state to stdout.
