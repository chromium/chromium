kstaled is a replacement for kswapd on ChromeOS. It will result in more
proactive swapping based on a configurable ratio. The default ratio is 4
and a ratio of 0 will disable the feature. For more information, see
the ChromeOS kernel source include/linux/kstaled.h.
