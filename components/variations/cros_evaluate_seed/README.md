# `cros_evaluate_seed`

This directory is for code to evaluate field trial state early in boot on
ChromeOS. All code here will be used in a single program, `evaluate_seed`. It is
not for general use in ash (or lacros).

## `evaluate_seed`

The executable `evaluate_seed` is a minimal program (budget: 2MiB of disk space
for the executable itself) used early in ChromeOS boot to determine which group
each early-boot experiment should be in, as well as any parameters for the
experiment. It lives here so that it is trivial to keep the code in sync between
ChromeOS's platform layer and chrome.

It will be built alongside ash.

It will be executed primarily by `featured`, which lives in
`//platform2/featured/`.

`evaluate_seed` will write a serialized version of the computed state to stdout,
along with a representation of the seed used for computation (for purposes of
determining whether the seed can be marked as "safe").

## Seed usage

When the device starts up, `featured` will exec `evaluate_seed`, which will by
default use whatever the latest seed in `/home/chronos/Local State` is. The
state computed by `evaluate_seed` will be cached in a tmpfs until the next
reboot, so even if ash later downloads and applies different seeds,
`evaluate_seed` will not re-evaluate the seed until the next device reboot.

For disaster recovery, featured will determine whether to use the safe (or null)
seed, and pass that information along with the value of the safe seed to use
along to `evaluate_seed`.
